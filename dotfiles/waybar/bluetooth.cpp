// Standalone GTK layer-shell popup; Waybar just launches it.
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace {
constexpr auto bluez = "org.bluez";
constexpr auto adapter_iface = "org.bluez.Adapter1";
constexpr auto device_iface = "org.bluez.Device1";
constexpr auto agent_path = "/io/github/peakyet/WaybarBluetooth/Agent";
constexpr auto agent_xml = R"(<node><interface name='org.bluez.Agent1'>
    <method name='Release'/><method name='Cancel'/>
    <method name='RequestPinCode'><arg type='o' direction='in'/><arg type='s' direction='out'/></method>
    <method name='DisplayPinCode'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>
    <method name='RequestPasskey'><arg type='o' direction='in'/><arg type='u' direction='out'/></method>
    <method name='DisplayPasskey'><arg type='o' direction='in'/><arg type='u' direction='in'/><arg type='q' direction='in'/></method>
    <method name='RequestConfirmation'><arg type='o' direction='in'/><arg type='u' direction='in'/></method>
    <method name='RequestAuthorization'><arg type='o' direction='in'/></method>
    <method name='AuthorizeService'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>
</interface></node>)";

GtkWidget* label(const char* text, const char* style = nullptr) {
    auto widget = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(widget), 0);
    if (style) gtk_style_context_add_class(gtk_widget_get_style_context(widget), style);
    return widget;
}

void pack(GtkWidget* box, GtkWidget* child, bool expand = false) {
    gtk_box_pack_start(GTK_BOX(box), child, expand, expand, 0);
}

void clear(GtkWidget* box) {
    gtk_container_foreach(GTK_CONTAINER(box), [](GtkWidget* child, gpointer) { gtk_widget_destroy(child); }, nullptr);
}

bool flag(GDBusProxy* proxy, const char* key) {
    g_autoptr(GVariant) value = g_dbus_proxy_get_cached_property(proxy, key);
    return value && g_variant_get_boolean(value);
}

std::string text(GDBusProxy* proxy, const char* key) {
    g_autoptr(GVariant) value = g_dbus_proxy_get_cached_property(proxy, key);
    return value ? g_variant_get_string(value, nullptr) : "";
}

struct Device {
    std::string path, name, address, icon;
    int group; // Connected, paired, nearby (in display order).

    auto order() const { return std::tie(group, name, path); }
    bool operator==(const Device& other) const {
        return order() == other.order() && address == other.address && icon == other.icon;
    }
};

struct Panel {
    GtkWidget *window = nullptr, *scan_button, *power_button, *rows, *status, *prompt;
    GDBusObjectManager* manager = nullptr;
    GDBusConnection* bus = nullptr; // Owned by manager.
    guint agent_id = 0, scan_timer = 0;
    bool agent_registered = false, busy = false, scan_pending = false;
    bool rendered_powered = false, rendered_busy = false;
    std::string adapter, scan_adapter, pairing, pairing_address, rendered_hint;
    std::vector<Device> shown;
    GDBusMethodInvocation* confirmation = nullptr;

    ~Panel() {
        if (scan_timer) g_source_remove(scan_timer);
        if (agent_id) g_dbus_connection_unregister_object(bus, agent_id);
        g_clear_object(&manager);
    }

    void message(const char* value) {
        gtk_label_set_text(GTK_LABEL(status), value);
        gtk_widget_set_visible(status, *value != '\0');
    }

    void error(GError* value) {
        g_dbus_error_strip_remote_error(value);
        message(value->message);
    }

    void show_dismiss_layer() {
        auto backdrop = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_set_name(backdrop, "bluetooth-popup");
        gtk_widget_set_visual(backdrop, gdk_screen_get_rgba_visual(gtk_widget_get_screen(backdrop)));
        auto layer = GTK_WINDOW(backdrop);
        gtk_layer_init_for_window(layer);
        gtk_layer_set_namespace(layer, "waybar-bluetooth-dismiss");
        gtk_layer_set_layer(layer, GTK_LAYER_SHELL_LAYER_TOP);
        // Cover reserved bar space as well, without moving the popup.
        gtk_layer_set_exclusive_zone(layer, -1);
        for (auto edge : {GTK_LAYER_SHELL_EDGE_TOP, GTK_LAYER_SHELL_EDGE_BOTTOM,
                          GTK_LAYER_SHELL_EDGE_LEFT, GTK_LAYER_SHELL_EDGE_RIGHT}) {
            gtk_layer_set_anchor(layer, edge, true);
        }
        gtk_widget_add_events(backdrop, GDK_BUTTON_PRESS_MASK);
        g_signal_connect(backdrop, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton*, GtkWidget* popup) -> gboolean {
            gtk_widget_destroy(popup);
            return true;
        }), window);
        g_signal_connect_object(window, "destroy", G_CALLBACK(gtk_widget_destroy), backdrop, G_CONNECT_SWAPPED);
        gtk_widget_show(backdrop);
    }

    void open(GtkApplication* app) {
        if (window) {
            gtk_widget_destroy(window);
            return;
        }
        if (!gtk_layer_is_supported()) {
            g_printerr("Bluetooth popup needs a Wayland layer-shell compositor.\n");
            return;
        }
        g_autoptr(GtkCssProvider) css = gtk_css_provider_new();
        g_autofree char* path = g_build_filename(g_get_home_dir(), ".config", "waybar", "themes", "bluetooth.css", nullptr);
        g_autoptr(GError) css_error = nullptr;
        if (!gtk_css_provider_load_from_path(css, path, &css_error)) {
            g_printerr("%s\n", css_error->message);
            return;
        }
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        window = gtk_application_window_new(app);
        auto layer = GTK_WINDOW(window);
        gtk_window_set_decorated(layer, false);
        gtk_window_set_resizable(layer, false);
        gtk_widget_set_name(window, "bluetooth-popup");
        gtk_widget_set_visual(window, gdk_screen_get_rgba_visual(gtk_widget_get_screen(window)));
        gtk_layer_init_for_window(layer);
        gtk_layer_set_namespace(layer, "waybar-bluetooth");
        gtk_layer_set_layer(layer, GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(layer, GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_anchor(layer, GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_margin(layer, GTK_LAYER_SHELL_EDGE_RIGHT, 72);
        gtk_layer_set_keyboard_mode(layer, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        g_signal_connect(window, "destroy", G_CALLBACK(+[](GtkWidget*, Panel* self) { self->close(); }), this);
        g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventKey* event, gpointer) -> gboolean {
            if (event->keyval != GDK_KEY_Escape) return false;
            gtk_widget_destroy(widget);
            return true;
        }), nullptr);

        auto panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_size_request(panel, 354, -1);
        gtk_widget_set_name(panel, "bluetooth-panel");
        auto update_input_region = +[](GtkWidget* panel, GtkAllocation* size, GtkWidget* popup) {
            // Let clicks on the transparent margin reach the dismiss layer.
            auto context = gtk_widget_get_style_context(panel);
            GtkBorder margin;
            gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
            cairo_rectangle_int_t bounds = {size->x + margin.left, size->y + margin.top,
                size->width - margin.left - margin.right, size->height - margin.top - margin.bottom};
            auto region = cairo_region_create_rectangle(&bounds);
            gtk_widget_input_shape_combine_region(popup, region);
            cairo_region_destroy(region);
        };
        g_signal_connect(panel, "size-allocate", G_CALLBACK(update_input_region), window);
        gtk_container_add(GTK_CONTAINER(window), panel);

        auto header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        pack(header, label("Bluetooth", "title"), true);
        power_button = gtk_button_new_with_label("Turn on");
        g_signal_connect(power_button, "clicked", G_CALLBACK(+[](GtkButton*, Panel* self) { self->power(); }), this);
        pack(header, power_button);
        scan_button = gtk_button_new_with_label("Scan");
        g_signal_connect(scan_button, "clicked", G_CALLBACK(+[](GtkButton*, Panel* self) { self->scan(!self->scan_timer); }), this);
        pack(header, scan_button);
        auto close_button = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_tooltip_text(close_button, "Close");
        atk_object_set_name(gtk_widget_get_accessible(close_button), "Close");
        g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(gtk_widget_destroy), window);
        pack(header, close_button);
        pack(panel, header);

        auto scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
        gtk_scrolled_window_set_policy(scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_propagate_natural_height(scroll, true);
        gtk_scrolled_window_set_max_content_height(scroll, 270);
        rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_add(GTK_CONTAINER(scroll), rows);
        pack(panel, GTK_WIDGET(scroll));
        prompt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_style_context_add_class(gtk_widget_get_style_context(prompt), "prompt");
        gtk_widget_set_no_show_all(prompt, true);
        pack(panel, prompt);
        status = label(nullptr, "secondary");
        gtk_label_set_line_wrap(GTK_LABEL(status), true);
        gtk_label_set_max_width_chars(GTK_LABEL(status), 38);
        gtk_widget_set_no_show_all(status, true);
        pack(panel, status);

        g_autoptr(GError) dbus_error = nullptr;
        manager = g_dbus_object_manager_client_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, bluez, "/", nullptr, nullptr, nullptr, nullptr, &dbus_error);
        if (manager) {
            bus = g_dbus_object_manager_client_get_connection(G_DBUS_OBJECT_MANAGER_CLIENT(manager));
            auto objects_changed = +[](GDBusObjectManager*, GDBusObject*, Panel* self) { self->refresh(); };
            g_signal_connect(manager, "object-added", G_CALLBACK(objects_changed), this);
            g_signal_connect(manager, "object-removed", G_CALLBACK(objects_changed), this);
            g_signal_connect(manager, "notify::name-owner", G_CALLBACK(+[](GObject*, GParamSpec*, Panel* self) {
                self->agent_registered = false;
                self->refresh();
            }), this);
            g_signal_connect(manager, "interface-proxy-properties-changed", G_CALLBACK(+[](
                GDBusObjectManagerClient*, GDBusObjectProxy*, GDBusProxy*, GVariant*, const char* const*, Panel* self) {
                self->refresh();
            }), this);
        }
        refresh();
        if (dbus_error) error(dbus_error);
        show_dismiss_layer();
        gtk_widget_show_all(window);
    }

    GDBusProxy* controller() {
        if (!manager || adapter.empty()) return nullptr;
        return G_DBUS_PROXY(g_dbus_object_manager_get_interface(manager, adapter.c_str(), adapter_iface));
    }

    void refresh() {
        if (!window) return;
        auto objects = manager ? g_dbus_object_manager_get_objects(manager) : nullptr;
        g_autoptr(GDBusProxy) current = controller();
        // Keep the selected controller as long as it exists (e.g. while it powers off).
        if (!current) {
            adapter.clear();
            for (auto it = objects; it; it = it->next) {
                auto object = G_DBUS_OBJECT(it->data);
                g_autoptr(GDBusInterface) iface = g_dbus_object_get_interface(object, adapter_iface);
                if (!iface) continue;
                auto path = g_dbus_object_get_object_path(object);
                if (adapter.empty() || path < adapter) adapter = path;
            }
            current = controller();
        }
        bool powered = current && flag(current, "Powered");
        bool blocked = current && text(current, "PowerState") == "off-blocked";
        if (scan_timer && (!powered || scan_adapter != adapter)) {
            g_clear_handle_id(&scan_timer, g_source_remove);
            scan_adapter.clear();
        }
        gtk_widget_set_sensitive(power_button, current && !busy && !scan_pending);
        gtk_button_set_label(GTK_BUTTON(power_button), powered ? "Turn off" : "Turn on");
        gtk_widget_set_sensitive(scan_button, powered && !scan_pending && !busy);
        gtk_button_set_label(GTK_BUTTON(scan_button), scan_timer ? "Stop scan" : "Scan");

        std::vector<Device> devices;
        for (auto it = objects; it; it = it->next) {
            auto object = G_DBUS_OBJECT(it->data);
            g_autoptr(GDBusInterface) iface = g_dbus_object_get_interface(object, device_iface);
            if (!iface) continue;
            auto proxy = G_DBUS_PROXY(iface);
            if (text(proxy, "Adapter") != adapter) continue;
            int group = flag(proxy, "Connected") ? 0 : flag(proxy, "Paired") ? 1 : 2;
            if (group == 2 && !powered) continue;
            auto address = text(proxy, "Address");
            auto name = text(proxy, "Alias");
            devices.push_back({g_dbus_object_get_object_path(object), name.empty() ? address : name,
                               address, text(proxy, "Icon"), group});
        }
        g_list_free_full(objects, g_object_unref);
        std::sort(devices.begin(), devices.end(), [](const Device& a, const Device& b) { return a.order() < b.order(); });
        auto hint = !current ? "No Bluetooth adapter available." : blocked ? "Bluetooth is blocked by the radio switch." :
                    !powered ? "Bluetooth is off." : scan_timer ? "Scanning for nearby devices…" : "Scan to find nearby devices.";
        if (rendered_powered == powered && rendered_busy == busy && shown == devices && rendered_hint == hint) return;
        rendered_hint = hint;
        rendered_powered = powered;
        rendered_busy = busy;
        shown = std::move(devices);
        clear(rows);
        const char* sections[] = {"CONNECTED", "PAIRED DEVICES", "NEARBY DEVICES"};
        int group = -1;
        for (const auto& device : shown) {
            if (group != device.group) {
                group = device.group;
                pack(rows, label(sections[group], "caption"));
            }
            add_device(device, powered && !busy);
        }
        if (group < 2) {
            if (powered) pack(rows, label(sections[2], "caption"));
            pack(rows, label(hint, "secondary"));
        }
        gtk_widget_show_all(rows);
    }

    void add_device(const Device& device, bool enabled) {
        auto row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        auto style = gtk_widget_get_style_context(row);
        gtk_style_context_add_class(style, "device");
        if (device.group == 0) gtk_style_context_add_class(style, "connected");
        auto icon = device.icon.empty() ? "bluetooth-active-symbolic" : device.icon + "-symbolic";
        if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon.c_str())) icon = "bluetooth-active-symbolic";
        pack(row, gtk_image_new_from_icon_name(icon.c_str(), GTK_ICON_SIZE_MENU));
        auto details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        auto name = label(device.name.c_str());
        gtk_label_set_single_line_mode(GTK_LABEL(name), true);
        gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
        pack(details, name);
        pack(details, label(device.address.c_str(), "secondary"));
        pack(row, details, true);
        auto button = gtk_button_new_with_label(device.group == 0 ? "Disconnect" : device.group == 1 ? "Connect" : "Pair");
        gtk_widget_set_sensitive(button, enabled);
        g_object_set_data_full(G_OBJECT(button), "path", g_strdup(device.path.c_str()), g_free);
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, Panel* self) {
            auto path = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "path"));
            self->device_action(path);
        }), this);
        pack(row, button);
        pack(rows, row);
    }

    void scan(bool start) {
        if (scan_pending) return;
        std::string target = start ? adapter : scan_adapter;
        if (target.empty()) return;
        g_clear_handle_id(&scan_timer, g_source_remove);
        scan_adapter = start ? target : "";
        scan_pending = true;
        message("");
        refresh();
        g_dbus_connection_call(bus, bluez, target.c_str(), adapter_iface, start ? "StartDiscovery" : "StopDiscovery",
            nullptr, nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START, 10000, nullptr, +[](GObject* source, GAsyncResult* result, gpointer data) {
                auto self = static_cast<Panel*>(data);
                g_autoptr(GError) failure = nullptr;
                g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &failure);
                if (!self->window) return;
                self->scan_pending = false;
                if (failure) {
                    self->scan_adapter.clear();
                    self->error(failure);
                } else if (!self->scan_adapter.empty()) {
                    self->scan_timer = g_timeout_add_seconds(20, +[](gpointer data) -> gboolean {
                        auto self = static_cast<Panel*>(data);
                        self->scan_timer = 0;
                        self->scan(false);
                        return G_SOURCE_REMOVE;
                    }, self);
                }
                self->refresh();
            }, this);
    }

    void power() {
        g_autoptr(GDBusProxy) current = controller();
        if (!current || busy || scan_pending) return;
        bool on = !flag(current, "Powered");
        // BlueZ cannot clear a soft rfkill block. Reuse the old bar action's TLP tool when available.
        if (on && text(current, "PowerState") == "off-blocked") {
            constexpr auto tlp = "/usr/bin/bluetooth";
            if (g_file_test(tlp, G_FILE_TEST_IS_EXECUTABLE)) {
                g_autoptr(GError) failure = nullptr;
                g_autoptr(GSubprocess) child = g_subprocess_new(
                    static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE),
                    &failure, tlp, "on", nullptr);
                if (!child) { error(failure); return; }
                busy = true;
                message("Turning Bluetooth on…");
                refresh();
                g_subprocess_wait_check_async(child, nullptr, +[](GObject* source, GAsyncResult* result, gpointer data) {
                    auto self = static_cast<Panel*>(data);
                    auto ok = g_subprocess_wait_check_finish(G_SUBPROCESS(source), result, nullptr);
                    if (!self->window) return;
                    if (ok) {
                        self->set_power(true);
                        return;
                    }
                    self->busy = false;
                    self->message("Couldn’t unblock Bluetooth. Check the system radio switch.");
                    self->refresh();
                }, this);
                return;
            }
        }
        set_power(on);
    }

    void set_power(bool on) {
        if (adapter.empty()) {
            busy = false;
            message("No Bluetooth adapter available.");
            refresh();
            return;
        }
        busy = true;
        message(on ? "Turning Bluetooth on…" : "Turning Bluetooth off…");
        refresh();
        g_dbus_connection_call(bus, bluez, adapter.c_str(), "org.freedesktop.DBus.Properties", "Set",
            g_variant_new("(ssv)", adapter_iface, "Powered", g_variant_new_boolean(on)), nullptr,
            G_DBUS_CALL_FLAGS_NO_AUTO_START, 10000, nullptr, action_done, this);
    }

    void device_action(std::string path) {
        if (busy) return;
        g_autoptr(GDBusProxy) current = controller();
        g_autoptr(GDBusInterface) iface = g_dbus_object_manager_get_interface(manager, path.c_str(), device_iface);
        if (!current || !flag(current, "Powered") || !iface) return;
        auto device = G_DBUS_PROXY(iface);
        if (text(device, "Adapter") != adapter) return;
        const char* method;
        if (flag(device, "Connected")) {
            method = "Disconnect";
            message("Disconnecting…");
        } else if (flag(device, "Paired")) {
            method = "Connect";
            message("Connecting…");
        } else {
            if (!register_agent()) return;
            pairing = path;
            pairing_address = text(device, "Address");
            method = "Pair";
            message("Pairing…");
        }
        busy = true;
        refresh();
        g_dbus_connection_call(bus, bluez, path.c_str(), device_iface, method, nullptr, nullptr,
            G_DBUS_CALL_FLAGS_NO_AUTO_START, 120000, nullptr, action_done, this);
    }

    static void action_done(GObject* source, GAsyncResult* result, gpointer data) {
        auto self = static_cast<Panel*>(data);
        g_autoptr(GError) failure = nullptr;
        g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &failure);
        if (!self->window) return;
        self->busy = false;
        if (g_error_matches(failure, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) self->cancel_pairing();
        self->pairing.clear();
        self->finish_prompt();
        if (failure) self->error(failure);
        else self->message("");
        self->refresh();
    }

    bool register_agent() {
        g_autoptr(GError) failure = nullptr;
        if (!agent_id) {
            g_autoptr(GDBusNodeInfo) node = g_dbus_node_info_new_for_xml(agent_xml, nullptr);
            static const GDBusInterfaceVTable vtable = {agent_call, nullptr, nullptr, {nullptr}};
            agent_id = g_dbus_connection_register_object(bus, agent_path, node->interfaces[0], &vtable, this, nullptr, &failure);
            if (!agent_id) { error(failure); return false; }
        }
        if (!agent_registered) {
            // This agent is only for our own Pair calls, never the system default.
            g_autoptr(GVariant) reply = g_dbus_connection_call_sync(bus, bluez, "/org/bluez", "org.bluez.AgentManager1",
                "RegisterAgent", g_variant_new("(os)", agent_path, "KeyboardDisplay"), nullptr,
                G_DBUS_CALL_FLAGS_NO_AUTO_START, 3000, nullptr, &failure);
            if (!reply) { error(failure); return false; }
            agent_registered = true;
        }
        return true;
    }

    static void agent_call(GDBusConnection*, const char* sender, const char*, const char*, const char* method,
                           GVariant* params, GDBusMethodInvocation* invocation, gpointer data) {
        auto self = static_cast<Panel*>(data);
        g_autofree char* owner = g_dbus_object_manager_client_get_name_owner(G_DBUS_OBJECT_MANAGER_CLIENT(self->manager));
        auto reject = [&] { g_dbus_method_invocation_return_dbus_error(invocation, "org.bluez.Error.Rejected", "Not requested here"); };
        if (!self->window || !owner || g_strcmp0(sender, owner) != 0) { reject(); return; }
        std::string request(method);
        if (request == "Release" || request == "Cancel") {
            if (request == "Release") self->agent_registered = false;
            self->finish_prompt();
            g_dbus_method_invocation_return_value(invocation, nullptr);
            return;
        }
        const char* device;
        g_variant_get_child(params, 0, "&o", &device);
        if (self->pairing != device || self->confirmation || request == "AuthorizeService") { reject(); return; }

        std::string instruction;
        bool pin = request == "RequestPinCode", passkey = request == "RequestPasskey";
        bool display = request == "DisplayPinCode" || request == "DisplayPasskey";
        if (pin || passkey) {
            instruction = pin ? "Enter the PIN shown by the device." : "Enter the numeric passkey shown by the device.";
        } else if (request == "RequestAuthorization") {
            instruction = "Allow this device to pair? No code was provided to verify it.";
        } else if (request == "DisplayPinCode") {
            const char* code;
            g_variant_get_child(params, 1, "&s", &code);
            instruction = std::string("Type ") + code + " on the device, then press Enter.";
        } else {
            guint32 code;
            g_variant_get_child(params, 1, "u", &code);
            g_autofree char* digits = g_strdup_printf("%06u", code);
            instruction = std::string(display ? "Type " : "Confirm that the device shows ") + digits +
                          (display ? " on the device, then press Enter." : ".");
        }
        if (display) g_dbus_method_invocation_return_value(invocation, nullptr);
        else self->confirmation = G_DBUS_METHOD_INVOCATION(g_object_ref(invocation));
        self->show_prompt(self->pairing_address + "\n" + instruction, pin, passkey, display);
    }

    void show_prompt(const std::string& instruction, bool pin, bool passkey, bool display) {
        clear(prompt);
        auto question = label(instruction.c_str());
        gtk_label_set_line_wrap(GTK_LABEL(question), true);
        gtk_label_set_max_width_chars(GTK_LABEL(question), 38);
        pack(prompt, question);
        auto buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        auto cancel = gtk_button_new_with_label("Cancel");
        g_signal_connect(cancel, "clicked", G_CALLBACK(+[](GtkButton*, Panel* self) {
            if (!self->confirmation) self->cancel_pairing();
            self->finish_prompt();
        }), this);
        pack(buttons, cancel);
        if (!display) {
            auto accept = gtk_button_new_with_label(pin || passkey ? "Submit" : "Confirm");
            gtk_style_context_add_class(gtk_widget_get_style_context(accept), "primary");
            g_signal_connect(accept, "clicked", G_CALLBACK(+[](GtkButton* button, Panel* self) {
                auto entry = GTK_ENTRY(g_object_get_data(G_OBJECT(button), "entry"));
                GVariant* reply = nullptr;
                if (entry) {
                    auto value = gtk_entry_get_text(entry);
                    if (!*value) return;
                    if (gtk_entry_get_input_purpose(entry) == GTK_INPUT_PURPOSE_DIGITS) {
                        std::string digits(value);
                        if (digits.size() > 6 || !std::all_of(digits.begin(), digits.end(), [](char c) { return g_ascii_isdigit(c); })) {
                            self->message("Enter a passkey of up to six digits.");
                            return;
                        }
                        reply = g_variant_new("(u)", static_cast<guint32>(g_ascii_strtoull(value, nullptr, 10)));
                    } else {
                        if (std::string(value).size() > 16) { self->message("Enter a PIN of up to 16 bytes."); return; }
                        reply = g_variant_new("(s)", value);
                    }
                }
                g_dbus_method_invocation_return_value(self->confirmation, reply);
                g_clear_object(&self->confirmation);
                self->finish_prompt();
                self->message("Pairing…");
            }), this);
            if (pin || passkey) {
                auto entry = GTK_ENTRY(gtk_entry_new());
                gtk_entry_set_max_length(entry, pin ? 16 : 6);
                gtk_entry_set_input_purpose(entry, pin ? GTK_INPUT_PURPOSE_PASSWORD : GTK_INPUT_PURPOSE_DIGITS);
                gtk_entry_set_visibility(entry, false);
                gtk_entry_set_placeholder_text(entry, pin ? "PIN" : "Passkey");
                g_object_set_data(G_OBJECT(accept), "entry", entry);
                pack(prompt, GTK_WIDGET(entry));
            }
            pack(buttons, accept);
        }
        pack(prompt, buttons);
        gtk_widget_show(prompt);
        gtk_container_foreach(GTK_CONTAINER(prompt), [](GtkWidget* child, gpointer) { gtk_widget_show_all(child); }, nullptr);
        // Never give the approval button implicit keyboard focus.
        gtk_widget_grab_focus(cancel);
    }

    void finish_prompt() {
        if (confirmation) {
            g_dbus_method_invocation_return_dbus_error(confirmation, "org.bluez.Error.Canceled", "Pairing canceled");
            g_clear_object(&confirmation);
        }
        gtk_widget_hide(prompt);
        clear(prompt);
    }

    void cancel_pairing() {
        if (pairing.empty()) return;
        g_dbus_connection_call(bus, bluez, pairing.c_str(), device_iface, "CancelPairing", nullptr, nullptr,
                               G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, nullptr, nullptr, nullptr);
        pairing.clear();
    }

    void close() {
        window = nullptr;
        finish_prompt();
        if (!pairing.empty()) {
            cancel_pairing();
            g_dbus_connection_flush_sync(bus, nullptr, nullptr);
        }
        // BlueZ releases this client's discovery session and agent when we exit.
    }
};
} // namespace

int main(int argc, char** argv) {
    Panel panel;
    g_autoptr(GtkApplication) app = gtk_application_new("io.github.peakyet.WaybarBluetooth", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(+[](GtkApplication* app, Panel* panel) { panel->open(app); }), &panel);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
