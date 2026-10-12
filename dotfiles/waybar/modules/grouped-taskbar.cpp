// Waybar CFFI ABI v2: https://github.com/Alexays/Waybar/tree/0.15.0/resources/custom_modules/cffi_example
//
// Grouped dock for niri: one icon per app ID across every workspace. Icons sit
// in workspace order, so the dock reads left to right like the workspace band
// on the top bar, and they slide past each other as windows move between
// workspaces.
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gfiledescriptorbased.h>
#include <json/json.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

extern "C" {
extern const size_t wbcffi_version = 2;
struct wbcffi_init_info {
    void* obj;
    const char* waybar_version;
    GtkContainer* (*get_root_widget)(void*);
    void (*queue_update)(void*);
};
struct wbcffi_config_entry { const char* key; const char* value; };
}

struct Dock {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* menu = nullptr;
    GList* apps = g_app_info_get_all();
    GSubprocess* stream = nullptr;
    GIOChannel* channel = nullptr;
    guint watch = 0, retry = 0;
    std::map<Json::UInt64, Json::Value> windows;
    std::map<std::string, GtkWidget*> buttons;
    std::vector<Json::Value> workspaces;   // niri's list, sorted by index
    std::vector<GtkWidget*> separators;    // hairlines between workspace groups

    struct AppButton { Dock* dock; std::string app; };

    ~Dock() {
        if (watch) g_source_remove(watch);
        if (retry) g_source_remove(retry);
        if (menu) gtk_widget_destroy(menu);
        stop_stream();
        g_list_free_full(apps, g_object_unref);
        gtk_widget_destroy(box);
    }

    void stop_stream() {
        if (channel) g_io_channel_unref(channel);
        channel = nullptr;
        if (stream) {
            g_subprocess_force_exit(stream);
            g_object_unref(stream);
        }
        stream = nullptr;
    }

    void connect_stream() {
        GError* error = nullptr;
        stream = g_subprocess_new(GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                 G_SUBPROCESS_FLAGS_STDERR_SILENCE), &error,
                                 "niri", "msg", "-j", "event-stream", nullptr);
        if (!stream) {
            g_warning("Grouped dock: %s", error->message);
            g_error_free(error);
            reconnect();
            return;
        }
        int fd = g_file_descriptor_based_get_fd(
            G_FILE_DESCRIPTOR_BASED(g_subprocess_get_stdout_pipe(stream)));
        channel = g_io_channel_unix_new(fd);
        g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, nullptr);
        watch = g_io_add_watch(channel, GIOCondition(G_IO_IN | G_IO_HUP | G_IO_ERR),
            [](GIOChannel* channel, GIOCondition condition, gpointer data) -> gboolean {
                auto* dock = static_cast<Dock*>(data);
                bool changed = false;
                GIOStatus status;
                gchar* line = nullptr;
                while ((status = g_io_channel_read_line(channel, &line, nullptr,
                                                       nullptr, nullptr)) == G_IO_STATUS_NORMAL) {
                    Json::Value event;
                    Json::Reader reader;
                    if (reader.parse(line, event)) changed |= dock->handle_event(event);
                    g_free(line);
                }
                if (changed) dock->render();
                if ((condition & (G_IO_HUP | G_IO_ERR)) || status == G_IO_STATUS_EOF ||
                    status == G_IO_STATUS_ERROR) {
                    dock->watch = 0;
                    dock->stop_stream();
                    dock->reconnect();
                    return G_SOURCE_REMOVE;
                }
                return G_SOURCE_CONTINUE;
            }, this);
    }

    void reconnect() {
        retry = g_timeout_add_seconds(2, [](gpointer data) -> gboolean {
            auto* dock = static_cast<Dock*>(data);
            dock->retry = 0;
            dock->connect_stream();
            return G_SOURCE_REMOVE;
        }, this);
    }

    bool handle_event(const Json::Value& event) {
        if (event.isMember("WorkspacesChanged")) {
            adopt_workspaces(event["WorkspacesChanged"]["workspaces"]);
        } else if (event.isMember("WindowsChanged")) {
            windows.clear();
            for (const auto& window : event["WindowsChanged"]["windows"])
                windows[window["id"].asUInt64()] = window;
        } else if (event.isMember("WindowOpenedOrChanged")) {
            const auto& window = event["WindowOpenedOrChanged"]["window"];
            windows[window["id"].asUInt64()] = window;
        } else if (event.isMember("WindowClosed")) {
            if (menu) gtk_widget_destroy(menu);
            windows.erase(event["WindowClosed"]["id"].asUInt64());
        } else if (event.isMember("WindowFocusChanged")) {
            const auto& id = event["WindowFocusChanged"]["id"];
            for (auto& [wid, window] : windows)
                window["is_focused"] = !id.isNull() && wid == id.asUInt64();
        } else if (event.isMember("WindowUrgencyChanged")) {
            const auto& change = event["WindowUrgencyChanged"];
            auto window = windows.find(change["id"].asUInt64());
            if (window != windows.end()) window->second["is_urgent"] = change["urgent"];
        } else {
            return false;
        }
        return true;
    }

    void adopt_workspaces(const Json::Value& list) {
        workspaces.clear();
        for (const auto& workspace : list) workspaces.push_back(workspace);
        std::sort(workspaces.begin(), workspaces.end(),
                  [](const Json::Value& a, const Json::Value& b) {
                      return a["idx"].asInt() < b["idx"].asInt();
                  });
    }

    // How far along the workspace band a workspace sits. Windows on a workspace
    // niri has not announced yet share the last rank, so they collect at the end
    // of the dock instead of jumping around.
    int workspace_rank(Json::UInt64 workspace_id) const {
        int rank = 0;
        for (const auto& workspace : workspaces) {
            if (workspace["id"].asUInt64() == workspace_id) return rank;
            ++rank;
        }
        return rank;
    }

    // Where an app sits in the dock: the earliest workspace holding one of its
    // windows, then the ID of its first window. An app spanning several
    // workspaces takes the earliest one, so it keeps its place among the apps of
    // that workspace rather than drifting to the end.
    std::pair<int, Json::UInt64> dock_order(const std::string& app) const {
        constexpr auto kLast = std::numeric_limits<Json::UInt64>::max();
        int rank = std::numeric_limits<int>::max();
        Json::UInt64 first = kLast;
        for (const auto& [id, window] : windows) {
            if (window["app_id"].asString() != app) continue;
            rank = std::min(rank, workspace_rank(window["workspace_id"].asUInt64()));
            first = std::min(first, id);
        }
        return {rank, first};
    }

    std::vector<Json::Value> group(const std::string& app) const {
        std::vector<Json::Value> result;
        for (const auto& [id, window] : windows)
            if (window["app_id"].asString() == app) result.push_back(window);
        std::sort(result.begin(), result.end(), [this](const Json::Value& a,
                                                      const Json::Value& b) {
            return std::make_pair(workspace_rank(a["workspace_id"].asUInt64()),
                                  a["id"].asUInt64()) <
                   std::make_pair(workspace_rank(b["workspace_id"].asUInt64()),
                                  b["id"].asUInt64());
        });
        return result;
    }

    GtkWidget* icon(const std::string& app) const {
        for (GList* item = apps; item; item = item->next) {
            if (!G_IS_DESKTOP_APP_INFO(item->data)) continue;
            auto* info = G_DESKTOP_APP_INFO(item->data);
            const char* wmclass = g_desktop_app_info_get_startup_wm_class(info);
            const char* id = g_app_info_get_id(G_APP_INFO(info));
            if ((wmclass && !g_ascii_strcasecmp(wmclass, app.c_str())) ||
                (id && !g_ascii_strcasecmp(id, (app + ".desktop").c_str()))) {
                if (GIcon* icon = g_app_info_get_icon(G_APP_INFO(info))) {
                    GtkWidget* image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG);
                    gtk_image_set_pixel_size(GTK_IMAGE(image), 28);
                    return image;
                }
            }
        }
        GtkWidget* image = gtk_image_new_from_icon_name(app.c_str(), GTK_ICON_SIZE_DIALOG);
        if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), app.c_str()))
            gtk_image_set_from_icon_name(GTK_IMAGE(image), "application-x-executable",
                                         GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 28);
        return image;
    }

    static void action(const char* name, Json::UInt64 id) {
        std::string number = std::to_string(id);
        const char* argv[] = {"niri", "msg", "action", name, "--id", number.c_str(), nullptr};
        GError* error = nullptr;
        if (!g_spawn_async(nullptr, const_cast<char**>(argv), nullptr, G_SPAWN_SEARCH_PATH,
                           nullptr, nullptr, nullptr, &error)) {
            g_warning("Grouped dock: %s", error->message);
            g_error_free(error);
        }
    }

    void choose(GtkWidget* button, const std::string& app) {
        auto members = group(app);
        if (members.empty()) return;
        if (members.size() == 1) {
            action("focus-window", members.front()["id"].asUInt64());
            return;
        }
        if (menu) gtk_widget_destroy(menu);
        menu = gtk_menu_new();
        gtk_widget_set_name(menu, "dock-window-menu");
        gtk_menu_attach_to_widget(GTK_MENU(menu), button, nullptr);
        auto* bar = GTK_WINDOW(gtk_widget_get_toplevel(box));
        // Waybar normally rejects keyboard focus; accept it only while choosing.
        gtk_layer_set_keyboard_mode(bar, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        g_signal_connect(menu, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* dock = static_cast<Dock*>(data);
            dock->menu = nullptr;
            auto* bar = GTK_WINDOW(gtk_widget_get_toplevel(dock->box));
            gtk_layer_set_keyboard_mode(bar, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        }), this);
        for (const auto& window : members) {
            GtkWidget* row = gtk_menu_item_new();
            GtkWidget* label = gtk_label_new(nullptr);
            std::string title = (window["is_focused"].asBool() ? "●  " : "    ") +
                                window["title"].asString();
            if (window["title"].asString().empty()) title += app;
            gtk_label_set_text(GTK_LABEL(label), title.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 55);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_widget_set_tooltip_text(row, window["title"].asCString());
            gtk_container_add(GTK_CONTAINER(row), label);
            auto* id = new Json::UInt64(window["id"].asUInt64());
            g_signal_connect_data(row, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer data) {
                action("focus-window", *static_cast<Json::UInt64*>(data));
            }), id, [](gpointer data, GClosure*) { delete static_cast<Json::UInt64*>(data); },
            GConnectFlags(0));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), row);
        }
        g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
        gtk_widget_show_all(menu);
        GdkEvent* event = gtk_get_current_event();
        // Accessibility activation has no pointer event; supply one for the Wayland popup grab.
        if (!event) {
            event = gdk_event_new(GDK_BUTTON_PRESS);
            event->button.window = GDK_WINDOW(g_object_ref(gtk_widget_get_window(button)));
            event->button.time = GDK_CURRENT_TIME;
            event->button.button = 1;
            gdk_event_set_device(event, gdk_seat_get_pointer(
                gdk_display_get_default_seat(gtk_widget_get_display(button))));
        }
        gtk_menu_popup_at_widget(GTK_MENU(menu), button, GDK_GRAVITY_NORTH,
                                 GDK_GRAVITY_SOUTH, event);
        gdk_event_free(event);
    }

    // Keep one separator per workspace boundary, in a pool that grows and
    // shrinks with the dock. render() reparents them; they are children of the
    // box, so the box destroys them with everything else.
    void separators_to(size_t count) {
        while (separators.size() > count) {
            gtk_widget_destroy(separators.back());
            separators.pop_back();
        }
        while (separators.size() < count) {
            GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
            gtk_style_context_add_class(gtk_widget_get_style_context(separator), "dock-separator");
            gtk_widget_set_valign(separator, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(box), separator, FALSE, FALSE, 0);
            gtk_widget_show(separator);
            separators.push_back(separator);
        }
    }

    void render() {
        for (auto it = buttons.begin(); it != buttons.end();) {
            if (group(it->first).empty()) {
                if (menu) gtk_widget_destroy(menu);
                gtk_widget_destroy(it->second);
                it = buttons.erase(it);
            } else ++it;
        }
        for (const auto& [id, window] : windows) {
            std::string app = window["app_id"].asString();
            if (buttons.count(app)) continue;
            GtkWidget* button = gtk_button_new();
            gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
            GtkWidget* overlay = gtk_overlay_new();
            GtkWidget* image = icon(app);
            gtk_container_add(GTK_CONTAINER(overlay), image);
            g_object_set_data(G_OBJECT(button), "icon", image);
            GtkWidget* count = gtk_label_new(nullptr);
            gtk_style_context_add_class(gtk_widget_get_style_context(count), "window-count");
            gtk_widget_set_halign(count, GTK_ALIGN_END);
            gtk_widget_set_valign(count, GTK_ALIGN_END);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), count);
            gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), count, TRUE);
            gtk_container_add(GTK_CONTAINER(button), overlay);
            g_object_set_data(G_OBJECT(button), "count", count);
            auto* data = new AppButton{this, app};
            g_signal_connect_data(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* app = static_cast<AppButton*>(data);
                app->dock->choose(GTK_WIDGET(button), app->app);
            }), data, [](gpointer data, GClosure*) { delete static_cast<AppButton*>(data); },
            GConnectFlags(0));
            gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
            buttons[app] = button;
            gtk_widget_show_all(button);
        }
        for (auto& [app, button] : buttons) {
            auto members = group(app);
            auto* style = gtk_widget_get_style_context(button);
            for (const char* state : {"active", "urgent"}) {
                bool set = std::any_of(members.begin(), members.end(), [state](const auto& window) {
                    return window[std::string("is_") + (std::string(state) == "active" ?
                                  "focused" : "urgent")].asBool();
                });
                if (set) gtk_style_context_add_class(style, state);
                else gtk_style_context_remove_class(style, state);
            }
            auto* count = GTK_LABEL(g_object_get_data(G_OBJECT(button), "count"));
            gtk_label_set_text(count, std::to_string(members.size()).c_str());
            gtk_widget_set_visible(GTK_WIDGET(count), members.size() > 1);
            std::string tooltip = app + " · " + std::to_string(members.size()) + " window(s)";
            if (members.size() == 1) tooltip = members.front()["title"].asString();
            gtk_widget_set_tooltip_text(button, tooltip.c_str());
            atk_object_set_name(gtk_widget_get_accessible(button), app.c_str());
        }

        // Rearrange the existing children into dock order, with a separator
        // wherever the workspace changes. A button keeps its widget; only its
        // position changes, so nothing flickers or loses state.
        std::vector<std::string> order;
        order.reserve(buttons.size());
        for (const auto& [app, button] : buttons) order.push_back(app);
        std::sort(order.begin(), order.end(), [this](const std::string& a, const std::string& b) {
            return dock_order(a) < dock_order(b);
        });
        std::vector<int> ranks;
        ranks.reserve(order.size());
        for (const auto& app : order) ranks.push_back(dock_order(app).first);
        size_t boundaries = 0;
        for (size_t i = 1; i < ranks.size(); ++i) boundaries += ranks[i] != ranks[i - 1];
        separators_to(boundaries);
        int position = 0;
        size_t separator = 0;
        for (size_t i = 0; i < order.size(); ++i) {
            if (i > 0 && ranks[i] != ranks[i - 1])
                gtk_box_reorder_child(GTK_BOX(box), separators[separator++], position++);
            gtk_box_reorder_child(GTK_BOX(box), buttons.at(order[i]), position++);
        }

        // Distance follows the visible dock order, rather than the app-ID map.
        // Separators are children too, so the walk skips anything without an icon.
        std::vector<GtkWidget*> icons;
        GList* children = gtk_container_get_children(GTK_CONTAINER(box));
        int focused = -1;
        for (GList* item = children; item; item = item->next) {
            auto* widget = GTK_WIDGET(item->data);
            if (!g_object_get_data(G_OBJECT(widget), "icon")) continue;
            if (focused < 0 && gtk_style_context_has_class(gtk_widget_get_style_context(widget),
                                                           "active"))
                focused = static_cast<int>(icons.size());
            icons.push_back(widget);
        }
        g_list_free(children);
        // Focused, neighbour, and two-or-more-away icon sizes. The pane's
        // content box is 48px tall and the focused button adds 4px of padding
        // on each side, so 40 is the largest the focused icon can be before it
        // starts stretching the dock past its configured height.
        const int sizes[] = {36, 22, 18};
        for (size_t i = 0; i < icons.size(); ++i) {
            int distance = focused < 0 ? 2 : std::min(std::abs(static_cast<int>(i) - focused), 2);
            gtk_image_set_pixel_size(GTK_IMAGE(g_object_get_data(G_OBJECT(icons[i]), "icon")),
                                     sizes[distance]);
        }
    }
};

extern "C" {
void* wbcffi_init(const wbcffi_init_info* info, const wbcffi_config_entry*, size_t) {
    auto* dock = new Dock;
    gtk_widget_set_name(dock->box, "taskbar");
    gtk_container_add(info->get_root_widget(info->obj), dock->box);
    gtk_widget_show(dock->box);
    dock->connect_stream();
    return dock;
}
void wbcffi_deinit(void* instance) { delete static_cast<Dock*>(instance); }
void wbcffi_update(void*) {}
void wbcffi_refresh(void*, int) {}
void wbcffi_doaction(void*, const char*) {}
}
