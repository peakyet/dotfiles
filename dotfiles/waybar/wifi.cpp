// Standalone GTK layer-shell popup; Waybar just launches it.
#include <NetworkManager.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {

enum class Security { Open, Psk, Sae, Owe, Other };

constexpr auto scanning_text = "Scanning for networks…";

Security security(NMAccessPoint* ap) {
    auto flags = nm_access_point_get_wpa_flags(ap) | nm_access_point_get_rsn_flags(ap);
    if (flags & NM_802_11_AP_SEC_KEY_MGMT_PSK) return Security::Psk;
    if (flags & NM_802_11_AP_SEC_KEY_MGMT_SAE) return Security::Sae;
    if (flags & NM_802_11_AP_SEC_KEY_MGMT_OWE) return Security::Owe;
    if (flags || (nm_access_point_get_flags(ap) & NM_802_11_AP_FLAGS_PRIVACY)) return Security::Other;
    return Security::Open;
}

std::string ssid_name(NMAccessPoint* ap) {
    gsize size;
    auto bytes = static_cast<const guint8*>(g_bytes_get_data(nm_access_point_get_ssid(ap), &size));
    g_autofree char* name = nm_utils_ssid_to_utf8(bytes, size);
    return name ? name : "";
}

GtkWidget* label(const char* text) {
    auto widget = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(widget), 0);
    return widget;
}

GtkWidget* icon(const char* name) {
    return gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_MENU);
}

void pack(GtkWidget* box, GtkWidget* child, bool expand = false) {
    gtk_box_pack_start(GTK_BOX(box), child, expand, expand, 0);
}

void style(GtkWidget* widget, const char* name) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

void clear(GtkWidget* box) {
    gtk_container_foreach(GTK_CONTAINER(box), [](GtkWidget* child, gpointer) {
        gtk_widget_destroy(child);
    }, nullptr);
}

GtkWidget* network_row(NMAccessPoint* ap, bool connected = false) {
    auto row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    style(row, "network-row");
    int strength = nm_access_point_get_strength(ap);
    pack(row, icon(strength >= 75 ? "network-wireless-signal-excellent-symbolic" :
                  strength >= 50 ? "network-wireless-signal-good-symbolic" :
                  strength >= 25 ? "network-wireless-signal-ok-symbolic" :
                                   "network-wireless-signal-weak-symbolic"));
    auto name = label(ssid_name(ap).c_str());
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(name), true);
    pack(row, name, true);
    if (connected) {
        auto badge = gtk_label_new("Connected");
        style(row, "current");
        style(badge, "badge");
        gtk_box_pack_end(GTK_BOX(row), badge, false, false, 0);
    } else if (security(ap) != Security::Open) {
        gtk_box_pack_end(GTK_BOX(row), icon("changes-prevent-symbolic"), false, false, 0);
    }
    return row;
}

struct Panel {
    GtkWidget* window = nullptr;
    GtkWidget* current;
    GtkWidget* networks;
    GtkWidget* rescan;
    GtkWidget* prompt;
    GtkWidget* status;
    NMClient* client = nullptr;
    NMDeviceWifi* device = nullptr;
    NMActiveConnection* pending = nullptr;
    GCancellable* cancellable = g_cancellable_new();

    ~Panel() {
        g_cancellable_cancel(cancellable);
        if (pending) g_signal_handlers_disconnect_by_data(pending, this);
        if (device) g_signal_handlers_disconnect_by_data(device, this);
        if (client) g_signal_handlers_disconnect_by_data(client, this);
        g_clear_object(&pending);
        g_clear_object(&device);
        g_clear_object(&client);
        g_object_unref(cancellable);
    }

    void open(GtkApplication* app) {
        if (window) {
            gtk_widget_destroy(window);
            return;
        }
        g_autoptr(GtkCssProvider) css = gtk_css_provider_new();
        g_autofree char* path = g_build_filename(g_get_home_dir(), ".config", "waybar", "themes", "wifi.css", nullptr);
        g_autoptr(GError) css_error = nullptr;
        if (!gtk_css_provider_load_from_path(css, path, &css_error)) {
            g_printerr("%s\n", css_error->message);
            return;
        }
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        window = gtk_application_window_new(app);
        gtk_window_set_decorated(GTK_WINDOW(window), false);
        gtk_window_set_resizable(GTK_WINDOW(window), false);
        gtk_widget_set_name(window, "wifi-popup");
        gtk_widget_set_visual(window, gdk_screen_get_rgba_visual(gtk_widget_get_screen(window)));
        gtk_layer_init_for_window(GTK_WINDOW(window));
        gtk_layer_set_namespace(GTK_WINDOW(window), "waybar-wifi");
        gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 72);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget*, Panel* self) {
            self->window = nullptr;
            g_cancellable_cancel(self->cancellable);
        }), this);
        g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventKey* event, gpointer) -> gboolean {
            if (event->keyval != GDK_KEY_Escape) return false;
            gtk_widget_destroy(widget);
            return true;
        }), nullptr);

        auto panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_size_request(panel, 324, -1);
        gtk_widget_set_name(panel, "wifi-panel");
        gtk_container_add(GTK_CONTAINER(window), panel);
        auto header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        auto title = label("Wi-Fi");
        gtk_widget_set_name(title, "wifi-title");
        pack(header, title, true);
        rescan = gtk_button_new_with_label("Rescan");
        gtk_button_set_image(GTK_BUTTON(rescan), icon("view-refresh-symbolic"));
        gtk_button_set_always_show_image(GTK_BUTTON(rescan), true);
        g_signal_connect(rescan, "clicked", G_CALLBACK(+[](GtkButton*, Panel* self) { self->scan(); }), this);
        pack(header, rescan);
        auto close = gtk_button_new();
        gtk_button_set_image(GTK_BUTTON(close), icon("window-close-symbolic"));
        gtk_widget_set_tooltip_text(close, "Close");
        atk_object_set_name(gtk_widget_get_accessible(close), "Close");
        g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_widget_destroy), window);
        pack(header, close);
        pack(panel, header);

        current = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        pack(panel, current);
        auto caption = label("NEARBY NETWORKS");
        gtk_widget_set_name(caption, "wifi-caption");
        pack(panel, caption);
        auto scroller = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
        gtk_scrolled_window_set_policy(scroller, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_propagate_natural_height(scroller, true);
        gtk_scrolled_window_set_max_content_height(scroller, 238);
        networks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_container_add(GTK_CONTAINER(scroller), networks);
        pack(panel, GTK_WIDGET(scroller));

        prompt = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_no_show_all(prompt, true);
        pack(panel, prompt);
        status = label(nullptr);
        gtk_label_set_line_wrap(GTK_LABEL(status), true);
        gtk_label_set_max_width_chars(GTK_LABEL(status), 37);
        gtk_widget_set_name(status, "wifi-status");
        gtk_widget_set_no_show_all(status, true);
        pack(panel, status);

        client = nm_client_new(nullptr, nullptr);
        if (client) {
            auto devices = nm_client_get_devices(client);
            for (guint i = 0; i < devices->len; ++i) {
                auto candidate = g_ptr_array_index(devices, i);
                if (!NM_IS_DEVICE_WIFI(candidate)) continue;
                auto wifi = NM_DEVICE_WIFI(candidate);
                if (!device || nm_device_wifi_get_active_access_point(wifi)) g_set_object(&device, wifi);
                if (nm_device_wifi_get_active_access_point(wifi)) break;
            }
            g_signal_connect(client, "notify::wireless-enabled", G_CALLBACK(refresh_signal), this);
        }
        if (device) {
            g_signal_connect(device, "notify::active-access-point", G_CALLBACK(refresh_signal), this);
            g_signal_connect(device, "notify::last-scan", G_CALLBACK(+[](GObject*, GParamSpec*, Panel* self) {
                if (!self->window) return;
                if (g_str_equal(gtk_label_get_text(GTK_LABEL(self->status)), scanning_text)) self->message("");
                self->refresh();
            }), this);
        }
        refresh();
        gtk_widget_show_all(window);
    }

    static void refresh_signal(GObject*, GParamSpec*, Panel* self) {
        if (self->window) self->refresh();
    }

    void message(const char* text) {
        gtk_label_set_text(GTK_LABEL(status), text);
        gtk_widget_set_visible(status, *text != '\0');
    }

    void refresh() {
        clear(current);
        clear(networks);
        bool available = device && nm_client_wireless_get_enabled(client);
        gtk_widget_set_sensitive(rescan, available);
        if (!available) {
            gtk_container_add(GTK_CONTAINER(networks), label("Wi-Fi is off or unavailable."));
        } else {
            std::set<std::pair<std::string, Security>> seen;
            auto remember = [&seen](NMAccessPoint* ap) {
                gsize size;
                auto bytes = static_cast<const char*>(g_bytes_get_data(nm_access_point_get_ssid(ap), &size));
                return seen.emplace(std::string(bytes, size), security(ap)).second;
            };
            auto active = nm_device_wifi_get_active_access_point(device);
            if (active && nm_access_point_get_ssid(active)) {
                gtk_container_add(GTK_CONTAINER(current), network_row(active, true));
                remember(active);
            }
            auto access_points = nm_device_wifi_get_access_points(device);
            std::vector<NMAccessPoint*> nearby;
            for (guint i = 0; i < access_points->len; ++i) nearby.push_back(NM_ACCESS_POINT(g_ptr_array_index(access_points, i)));
            std::stable_sort(nearby.begin(), nearby.end(), [](NMAccessPoint* a, NMAccessPoint* b) {
                return nm_access_point_get_strength(a) > nm_access_point_get_strength(b);
            });
            bool any = false;
            for (auto ap : nearby) {
                if (!nm_access_point_get_ssid(ap) || !remember(ap)) continue;
                auto button = gtk_button_new();
                gtk_container_add(GTK_CONTAINER(button), network_row(ap));
                g_object_set_data_full(G_OBJECT(button), "ap", g_object_ref(ap), g_object_unref);
                g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, Panel* self) {
                    self->choose(NM_ACCESS_POINT(g_object_get_data(G_OBJECT(button), "ap")));
                }), this);
                gtk_container_add(GTK_CONTAINER(networks), button);
                any = true;
            }
            if (!any) gtk_container_add(GTK_CONTAINER(networks), label("No other networks found."));
        }
        gtk_widget_show_all(current);
        gtk_widget_show_all(networks);
    }

    void scan() {
        gtk_widget_set_sensitive(rescan, false);
        message(scanning_text);
        nm_device_wifi_request_scan_async(device, cancellable, [](GObject* source, GAsyncResult* result, gpointer data) {
            auto self = static_cast<Panel*>(data);
            g_autoptr(GError) error = nullptr;
            if (!nm_device_wifi_request_scan_finish(NM_DEVICE_WIFI(source), result, &error) && self->window) {
                gtk_widget_set_sensitive(self->rescan, true);
                self->message(error->message);
            }
        }, this);
    }

    void choose(NMAccessPoint* ap) {
        gtk_widget_hide(prompt);
        clear(prompt);
        auto profiles = nm_device_get_available_connections(NM_DEVICE(device));
        for (guint i = 0; i < profiles->len; ++i) {
            auto profile = NM_CONNECTION(g_ptr_array_index(profiles, i));
            if (nm_access_point_connection_valid(ap, profile)) {
                join(ap, profile, nullptr);
                return;
            }
        }
        auto mode = security(ap);
        if (mode == Security::Open || mode == Security::Owe) {
            join(ap, nullptr, nullptr);
        } else if (mode == Security::Other) {
            message("Set up a NetworkManager profile for this network first.");
        } else {
            message(("Enter the password for " + ssid_name(ap) + ".").c_str());
            auto entry = GTK_ENTRY(gtk_entry_new());
            gtk_entry_set_visibility(entry, false);
            gtk_entry_set_placeholder_text(entry, "Wi-Fi password");
            atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(entry)), "Wi-Fi password");
            auto button = gtk_button_new_with_label("Join");
            gtk_widget_set_sensitive(button, false);
            style(button, "join");
            g_object_set_data_full(G_OBJECT(button), "ap", g_object_ref(ap), g_object_unref);
            g_object_set_data(G_OBJECT(button), "entry", entry);
            g_signal_connect(entry, "changed", G_CALLBACK(+[](GtkEntry* field, GtkWidget* button) {
                auto target = NM_ACCESS_POINT(g_object_get_data(G_OBJECT(button), "ap"));
                auto text = gtk_entry_get_text(field);
                gtk_widget_set_sensitive(button, security(target) == Security::Sae ? *text != '\0' : nm_utils_wpa_psk_valid(text));
            }), button);
            g_signal_connect(entry, "activate", G_CALLBACK(+[](GtkEntry*, GtkWidget* button) {
                if (gtk_widget_get_sensitive(button)) gtk_button_clicked(GTK_BUTTON(button));
            }), button);
            g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, Panel* self) {
                auto target = NM_ACCESS_POINT(g_object_get_data(G_OBJECT(button), "ap"));
                auto field = GTK_ENTRY(g_object_get_data(G_OBJECT(button), "entry"));
                self->join(target, nullptr, gtk_entry_get_text(field));
                gtk_entry_set_text(field, "");
                gtk_widget_hide(self->prompt);
            }), this);
            pack(prompt, GTK_WIDGET(entry), true);
            pack(prompt, button);
            gtk_widget_show(GTK_WIDGET(entry));
            gtk_widget_show(button);
            gtk_widget_show(prompt);
            gtk_widget_grab_focus(GTK_WIDGET(entry));
        }
    }

    void join(NMAccessPoint* ap, NMConnection* profile, const char* password) {
        gtk_widget_set_sensitive(networks, false);
        message("Joining network…");
        auto path = nm_object_get_path(NM_OBJECT(ap));
        if (profile) {
            nm_client_activate_connection_async(client, profile, NM_DEVICE(device), path, cancellable,
                [](GObject* source, GAsyncResult* result, gpointer data) {
                    g_autoptr(GError) error = nullptr;
                    auto active = nm_client_activate_connection_finish(NM_CLIENT(source), result, &error);
                    static_cast<Panel*>(data)->join_result(active, error);
                }, this);
            return;
        }
        g_autoptr(NMConnection) partial = nullptr;
        auto mode = security(ap);
        if (mode != Security::Open) {
            partial = nm_simple_connection_new();
            auto wifi = nm_setting_wireless_new();
            g_object_set(wifi, NM_SETTING_WIRELESS_SSID, nm_access_point_get_ssid(ap), nullptr);
            nm_connection_add_setting(partial, wifi);
            auto auth = nm_setting_wireless_security_new();
            auto method = mode == Security::Psk ? "wpa-psk" : mode == Security::Sae ? "sae" : "owe";
            g_object_set(auth, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, method,
                         NM_SETTING_WIRELESS_SECURITY_PSK, password, nullptr);
            nm_connection_add_setting(partial, auth);
        }
        nm_client_add_and_activate_connection_async(client, partial, NM_DEVICE(device), path, cancellable,
            [](GObject* source, GAsyncResult* result, gpointer data) {
                g_autoptr(GError) error = nullptr;
                auto active = nm_client_add_and_activate_connection_finish(NM_CLIENT(source), result, &error);
                static_cast<Panel*>(data)->join_result(active, error);
            }, this);
    }

    void join_result(NMActiveConnection* active, const GError* error) {
        g_autoptr(NMActiveConnection) result = active;
        if (!window) return;
        if (!active) {
            gtk_widget_set_sensitive(networks, true);
            message(error->message);
            return;
        }
        if (pending) g_signal_handlers_disconnect_by_data(pending, this);
        g_set_object(&pending, active);
        g_signal_connect(pending, "notify::state", G_CALLBACK(connection_changed), this);
        connection_changed(NM_ACTIVE_CONNECTION(pending), nullptr, this);
    }

    static void connection_changed(NMActiveConnection* active, GParamSpec*, Panel* self) {
        if (!self->window) return;
        auto state = nm_active_connection_get_state(active);
        if (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED) {
            gtk_widget_destroy(self->window);
        } else if (state >= NM_ACTIVE_CONNECTION_STATE_DEACTIVATING) {
            gtk_widget_set_sensitive(self->networks, true);
            self->message("Couldn’t join the network. Check the password or saved profile.");
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    Panel panel;
    g_autoptr(GtkApplication) app = gtk_application_new("io.github.peakyet.WaybarWifi", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(+[](GtkApplication* app, Panel* panel) { panel->open(app); }), &panel);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
