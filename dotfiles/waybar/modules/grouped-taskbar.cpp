// Waybar CFFI ABI v2: https://github.com/Alexays/Waybar/tree/0.15.0/resources/custom_modules/cffi_example
//
// Grouped dock for niri: one icon per app per workspace. Icons sit in workspace
// order, so the dock reads left to right like the workspace band on the top bar,
// with a hairline between the groups, and they slide past each other as windows
// move between workspaces. An app's windows stack under one icon only while they
// share a workspace. Inside a workspace the icons follow the layout itself: left
// to right across the columns, top to bottom down one column, with floating
// windows trailing the tiled ones.
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gfiledescriptorbased.h>
#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <tuple>
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
    // Where a window sits inside its workspace, as far as the dock cares: niri
    // hands tiled windows a column and a place in that column, and floating
    // windows a pixel position instead. The leading number keeps those two kinds
    // apart, so tiled windows always sort before floating ones; a window with
    // neither sorts last, and the window ID breaks every tie.
    using WindowOrder = std::tuple<int, double, double, Json::UInt64>;

    // One icon's place in the dock: its workspace's rank in the band, then the
    // position of its leading window inside that workspace.
    using DockOrder = std::pair<int, WindowOrder>;

    // One icon per app per workspace: windows of the same app stack only while
    // they sit on the same workspace, so an app on two workspaces gets an icon
    // in each of them rather than one icon spanning both.
    struct AppKey {
        std::string app;
        Json::UInt64 workspace;

        bool operator<(const AppKey& other) const {
            return std::tie(app, workspace) < std::tie(other.app, other.workspace);
        }
    };

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* menu = nullptr;
    GList* apps = g_app_info_get_all();
    GSubprocess* stream = nullptr;
    GIOChannel* channel = nullptr;
    guint watch = 0, retry = 0;
    std::map<Json::UInt64, Json::Value> windows;
    std::map<AppKey, GtkWidget*> buttons;
    std::vector<Json::Value> workspaces;   // niri's list, sorted by index
    std::vector<GtkWidget*> separators;    // hairlines between workspace groups

    struct AppButton {
        Dock* dock;
        AppKey key;
        GtkWidget* face;
        GIcon* icon;
        GdkPixbuf* pixels = nullptr;
        int scale = 0;
        double lift = 0;

        ~AppButton() {
            g_clear_object(&pixels);
            g_object_unref(icon);
        }

        void draw(cairo_t* cr) {
            constexpr int largest = 42;
            const int current_scale = gtk_widget_get_scale_factor(face);
            if (scale != current_scale) {
                g_clear_object(&pixels);
                scale = current_scale;
                auto* theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(face));
                if (auto* info = gtk_icon_theme_lookup_by_gicon_for_scale(
                        theme, icon, largest, scale, GTK_ICON_LOOKUP_FORCE_SIZE)) {
                    pixels = gtk_icon_info_load_icon(info, nullptr);
                    g_object_unref(info);
                }
                if (!pixels)
                    pixels = gtk_icon_theme_load_icon_for_scale(theme, "application-x-executable",
                        largest, scale, GTK_ICON_LOOKUP_FORCE_SIZE, nullptr);
            }
            if (!pixels) return;
            // Grow into the headroom above the icon, never outside its slot.
            const double size = 32 + (largest - 32) * lift;
            const double bottom = 48 - (48 - largest) * lift;
            const int width = gdk_pixbuf_get_width(pixels), height = gdk_pixbuf_get_height(pixels);
            const double ratio = size / std::max(width, height);
            cairo_translate(cr, (gtk_widget_get_allocated_width(face) - width * ratio) / 2,
                            bottom - (size + height * ratio) / 2);
            cairo_scale(cr, ratio, ratio);
            gdk_cairo_set_source_pixbuf(cr, pixels, 0, 0);
            cairo_paint(cr);
        }
    };
    std::vector<AppButton*> slots;         // visible order, excluding dividers
    AppButton* hovered = nullptr;
    guint lens_tick = 0;
    gint64 lens_frame = 0;
    cairo_surface_t* lens_tray = nullptr;  // static glass under the moving icons

    ~Dock() {
        if (watch) g_source_remove(watch);
        if (retry) g_source_remove(retry);
        if (lens_tick) gtk_widget_remove_tick_callback(box, lens_tick);
        g_clear_pointer(&lens_tray, cairo_surface_destroy);
        slots.clear();
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
            // A focused window opening (including a tray restore) need not
            // emit WindowFocusChanged. Its focus supersedes the cached one.
            if (window["is_focused"].asBool())
                for (auto& [id, previous] : windows) previous["is_focused"] = false;
            windows[window["id"].asUInt64()] = window;
        } else if (event.isMember("WindowClosed")) {
            if (menu) gtk_widget_destroy(menu);
            windows.erase(event["WindowClosed"]["id"].asUInt64());
        } else if (event.isMember("WindowLayoutsChanged")) {
            // Layout positions arrive separately from window properties, both on
            // open and whenever niri moves or resizes a tile, so the dock order
            // follows a column that moves.
            for (const auto& change : event["WindowLayoutsChanged"]["changes"]) {
                auto window = windows.find(change[0].asUInt64());
                if (window != windows.end()) window->second["layout"] = change[1];
            }
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

    // What to call a workspace in a tooltip: its name, or its index when it has
    // none. Empty when niri has not announced it yet.
    std::string workspace_label(Json::UInt64 workspace_id) const {
        for (const auto& workspace : workspaces) {
            if (workspace["id"].asUInt64() != workspace_id) continue;
            const std::string name = workspace["name"].asString();
            return name.empty() ? "workspace " + std::to_string(workspace["idx"].asInt()) : name;
        }
        return {};
    }

    // Where a window sits inside its workspace. niri reports a column and a
    // place in that column for tiled windows, and a pixel position for floating
    // ones; a window it has not laid out yet reports neither.
    static WindowOrder window_order(const Json::Value& window) {
        const auto& column = window["layout"]["pos_in_scrolling_layout"];
        if (column.isArray() && column.size() == 2)
            return {0, column[0].asDouble(), column[1].asDouble(), window["id"].asUInt64()};
        const auto& pixels = window["layout"]["tile_pos_in_workspace_view"];
        if (pixels.isArray() && pixels.size() == 2)
            return {1, pixels[0].asDouble(), pixels[1].asDouble(), window["id"].asUInt64()};
        return {2, 0, 0, window["id"].asUInt64()};
    }

    // Where an icon sits in the dock: its workspace's place in the band, then
    // the leftmost-topmost window of the group. No two icons lead with the same
    // window, so the order is total.
    DockOrder dock_order(const AppKey& key) const {
        // A sentinel past every real position; the key has a window by the time
        // its button exists.
        WindowOrder leading{std::numeric_limits<int>::max(), 0, 0, 0};
        for (const auto& [id, window] : windows)
            if (window["app_id"].asString() == key.app &&
                window["workspace_id"].asUInt64() == key.workspace)
                leading = std::min(leading, window_order(window));
        return {workspace_rank(key.workspace), leading};
    }

    // The windows behind one icon, in the order they sit in the workspace.
    std::vector<Json::Value> group(const AppKey& key) const {
        std::vector<Json::Value> result;
        for (const auto& [id, window] : windows)
            if (window["app_id"].asString() == key.app &&
                window["workspace_id"].asUInt64() == key.workspace)
                result.push_back(window);
        std::sort(result.begin(), result.end(), [](const Json::Value& a, const Json::Value& b) {
            return window_order(a) < window_order(b);
        });
        return result;
    }

    GIcon* icon(const std::string& app) const {
        for (GList* item = apps; item; item = item->next) {
            if (!G_IS_DESKTOP_APP_INFO(item->data)) continue;
            auto* info = G_DESKTOP_APP_INFO(item->data);
            const char* wmclass = g_desktop_app_info_get_startup_wm_class(info);
            const char* id = g_app_info_get_id(G_APP_INFO(info));
            if ((wmclass && !g_ascii_strcasecmp(wmclass, app.c_str())) ||
                (id && !g_ascii_strcasecmp(id, (app + ".desktop").c_str()))) {
                if (GIcon* icon = g_app_info_get_icon(G_APP_INFO(info)))
                    return G_ICON(g_object_ref(icon));
            }
        }
        return g_themed_icon_new(app.c_str());
    }

    bool advance_lens(double blend) {
        auto at = std::find(slots.begin(), slots.end(), menu ? nullptr : hovered);
        bool moving = false;
        for (auto it = slots.begin(); it != slots.end(); ++it) {
            auto& slot = **it;
            const double target = at == slots.end() ? 0 :
                it == at ? 1 : std::abs(it - at) == 1 ? 0.3 : 0;
            if (slot.lift == target) continue;
            const double next = slot.lift + (target - slot.lift) * blend;
            slot.lift = std::abs(target - next) < 0.015 ? target : next;
            moving |= slot.lift != target;
            gtk_widget_queue_draw(slot.face);
        }
        return moving;
    }

    void refresh_lens() {
        gboolean animations = TRUE;
        g_object_get(gtk_widget_get_settings(box), "gtk-enable-animations", &animations, nullptr);
        if (!advance_lens(animations ? 0 : 1) || lens_tick) return;
        lens_frame = 0;
        lens_tick = gtk_widget_add_tick_callback(box,
            +[](GtkWidget*, GdkFrameClock* clock, gpointer data) -> gboolean {
                auto& dock = *static_cast<Dock*>(data);
                const gint64 now = gdk_frame_clock_get_frame_time(clock);
                const double elapsed = dock.lens_frame ? now - dock.lens_frame : 16667;
                dock.lens_frame = now;
                if (dock.advance_lens(1 - std::exp(-elapsed / 35000))) return G_SOURCE_CONTINUE;
                dock.lens_tick = 0;
                g_clear_pointer(&dock.lens_tray, cairo_surface_destroy);
                return G_SOURCE_REMOVE;
            }, this, nullptr);
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

    void choose(GtkWidget* button, const AppKey& key) {
        auto members = group(key);
        if (members.empty()) return;
        if (members.size() == 1) {
            action("focus-window", members.front()["id"].asUInt64());
            return;
        }
        if (menu) gtk_widget_destroy(menu);
        menu = gtk_menu_new();
        refresh_lens();
        gtk_widget_set_name(menu, "dock-window-menu");
        gtk_menu_attach_to_widget(GTK_MENU(menu), button, nullptr);
        auto* bar = GTK_WINDOW(gtk_widget_get_toplevel(box));
        // Waybar normally rejects keyboard focus; accept it only while choosing.
        gtk_layer_set_keyboard_mode(bar, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        g_signal_connect(menu, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* dock = static_cast<Dock*>(data);
            dock->menu = nullptr;
            dock->refresh_lens();
            auto* bar = GTK_WINDOW(gtk_widget_get_toplevel(dock->box));
            gtk_layer_set_keyboard_mode(bar, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        }), this);
        for (const auto& window : members) {
            GtkWidget* row = gtk_menu_item_new();
            GtkWidget* label = gtk_label_new(nullptr);
            std::string title = (window["is_focused"].asBool() ? "●  " : "    ") +
                                window["title"].asString();
            if (window["title"].asString().empty()) title += key.app;
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
        slots.clear();
        for (auto it = buttons.begin(); it != buttons.end();) {
            if (group(it->first).empty()) {
                if (menu) gtk_widget_destroy(menu);
                if (hovered == g_object_get_data(G_OBJECT(it->second), "slot")) hovered = nullptr;
                gtk_widget_destroy(it->second);
                it = buttons.erase(it);
            } else ++it;
        }
        for (const auto& [id, window] : windows) {
            std::string app = window["app_id"].asString();
            AppKey key{app, window["workspace_id"].asUInt64()};
            if (buttons.count(key)) continue;
            GtkWidget* button = gtk_button_new();
            gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
            GtkWidget* overlay = gtk_overlay_new();
            gtk_widget_set_size_request(overlay, 46, 56);
            GtkWidget* tile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_style_context_add_class(gtk_widget_get_style_context(tile), "focus-tile");
            gtk_widget_set_margin_top(tile, 10);
            gtk_widget_set_margin_bottom(tile, 3);
            gtk_widget_set_margin_start(tile, 1);
            gtk_widget_set_margin_end(tile, 1);
            gtk_container_add(GTK_CONTAINER(overlay), tile);
            GtkWidget* face = gtk_drawing_area_new();
            gtk_widget_set_has_window(face, FALSE);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), face);
            gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), face, TRUE);
            auto* slot = new AppButton{this, key, face, icon(app)};
            g_object_set_data_full(G_OBJECT(button), "slot", slot,
                +[](gpointer data) { delete static_cast<AppButton*>(data); });
            g_object_set_data(G_OBJECT(face), "slot", slot);
            g_signal_connect(face, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer data) {
                static_cast<AppButton*>(data)->draw(cr);
                return FALSE;
            }), slot);
            g_signal_connect_object(gtk_icon_theme_get_default(), "changed",
                G_CALLBACK(+[](GtkWidget* face) {
                    static_cast<AppButton*>(g_object_get_data(G_OBJECT(face), "slot"))->scale = 0;
                    gtk_widget_queue_draw(face);
                }), face, G_CONNECT_SWAPPED);
            GtkWidget* dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_style_context_add_class(gtk_widget_get_style_context(dot), "running-dot");
            gtk_widget_set_halign(dot, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(dot, GTK_ALIGN_END);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), dot);
            gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), dot, TRUE);
            GtkWidget* count = gtk_label_new(nullptr);
            gtk_style_context_add_class(gtk_widget_get_style_context(count), "window-count");
            gtk_widget_set_halign(count, GTK_ALIGN_END);
            gtk_widget_set_valign(count, GTK_ALIGN_END);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), count);
            gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), count, TRUE);
            gtk_container_add(GTK_CONTAINER(button), overlay);
            g_object_set_data(G_OBJECT(button), "count", count);
            g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* app = static_cast<AppButton*>(data);
                app->dock->choose(GTK_WIDGET(button), app->key);
            }), slot);
            g_signal_connect(button, "state-flags-changed", G_CALLBACK(+[](
                GtkWidget* button, GtkStateFlags previous, gpointer data) {
                auto* slot = static_cast<AppButton*>(data);
                auto& dock = *slot->dock;
                const bool inside = gtk_widget_get_state_flags(button) & GTK_STATE_FLAG_PRELIGHT;
                if (inside == bool(previous & GTK_STATE_FLAG_PRELIGHT)) return;
                if (inside) dock.hovered = slot;
                else if (dock.hovered == slot) dock.hovered = nullptr;
                dock.refresh_lens();
            }), slot);
            gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
            buttons[key] = button;
            gtk_widget_show_all(button);
        }
        for (auto& [key, button] : buttons) {
            auto members = group(key);
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
            const std::string label = workspace_label(key.workspace);
            std::string tooltip;
            if (members.size() == 1) {
                tooltip = members.front()["title"].asString();
            } else {
                tooltip = key.app + " · " + std::to_string(members.size()) + " window(s)";
                if (!label.empty()) tooltip += " on " + label;
            }
            gtk_widget_set_tooltip_text(button, tooltip.c_str());
            // Two icons of one app differ only by workspace, so name it.
            const std::string name = label.empty() ? key.app : key.app + " on " + label;
            atk_object_set_name(gtk_widget_get_accessible(button), name.c_str());
        }

        // Rearrange the existing children into dock order, with a separator
        // wherever the workspace changes. A button keeps its widget; only its
        // position changes, so nothing flickers or loses state.
        std::vector<AppKey> order;
        order.reserve(buttons.size());
        for (const auto& [key, button] : buttons) order.push_back(key);
        std::sort(order.begin(), order.end(), [this](const AppKey& a, const AppKey& b) {
            return dock_order(a) < dock_order(b);
        });
        std::vector<int> ranks;
        ranks.reserve(order.size());
        for (const auto& key : order) ranks.push_back(dock_order(key).first);
        size_t boundaries = 0;
        for (size_t i = 1; i < ranks.size(); ++i) boundaries += ranks[i] != ranks[i - 1];
        separators_to(boundaries);
        int position = 0;
        size_t separator = 0;
        for (size_t i = 0; i < order.size(); ++i) {
            if (i > 0 && ranks[i] != ranks[i - 1])
                gtk_box_reorder_child(GTK_BOX(box), separators[separator++], position++);
            auto* button = buttons.at(order[i]);
            gtk_box_reorder_child(GTK_BOX(box), button, position++);
            slots.push_back(static_cast<AppButton*>(g_object_get_data(G_OBJECT(button), "slot")));
        }
        refresh_lens();
    }
};

extern "C" {
void* wbcffi_init(const wbcffi_init_info* info, const wbcffi_config_entry*, size_t) {
    auto* dock = new Dock;
    gtk_widget_set_name(dock->box, "taskbar");
    for (const char* signal : {"style-updated", "size-allocate", "notify::scale-factor"})
        g_signal_connect_swapped(dock->box, signal, G_CALLBACK(+[](Dock* dock) {
            g_clear_pointer(&dock->lens_tray, cairo_surface_destroy);
        }), dock);
    g_signal_connect(dock->box, "draw", G_CALLBACK(+[](GtkWidget* box, cairo_t* cr, gpointer data) {
        auto* dock = static_cast<Dock*>(data);
        if (dock->slots.empty()) return FALSE;
        // Leave the top of the layer transparent so the icon can rise above the tray.
        constexpr int lip = 8;
        const int width = gtk_widget_get_allocated_width(box);
        const int height = gtk_widget_get_allocated_height(box);
        auto paint = [&](cairo_t* target) {
            auto* style = gtk_widget_get_style_context(box);
            gtk_style_context_save(style);
            gtk_style_context_add_class(style, "aqua-tray");
            gtk_render_background(style, target, 0, lip, width, height - lip);
            gtk_render_frame(style, target, 0, lip, width, height - lip);
            gtk_style_context_restore(style);
        };
        if (dock->lens_tick) {
            if (!dock->lens_tray) {
                dock->lens_tray = gdk_window_create_similar_surface(gtk_widget_get_window(box),
                    CAIRO_CONTENT_COLOR_ALPHA, width, height);
                auto* target = cairo_create(dock->lens_tray);
                paint(target);
                cairo_destroy(target);
            }
            cairo_save(cr);
            cairo_set_source_surface(cr, dock->lens_tray, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        } else paint(cr);
        return FALSE; // GTK draws the buttons on top.
    }), dock);
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
