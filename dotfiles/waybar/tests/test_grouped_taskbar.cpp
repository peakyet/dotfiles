// Run with GTK's Wayland backend; no test windows or compositor actions are created.
#include "../modules/grouped-taskbar.cpp"

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);
    auto* css = gtk_css_provider_new();
    assert(gtk_css_provider_load_from_path(css, "dotfiles/waybar/style.css", nullptr));
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    g_object_set(gtk_settings_get_default(), "gtk-enable-animations", FALSE, nullptr);
    Dock dock;
    gtk_widget_set_name(dock.box, "taskbar");
    gtk_widget_show(dock.box);
    auto event = [&](const char* json) {
        Json::Value value;
        Json::Reader reader;
        assert(reader.parse(json, value));
        assert(dock.handle_event(value));
        dock.render();
    };
    // The same app on another workspace must remain a separate button with
    // its own focus state, regardless of which slot is hovered.
    auto check = [&](const char* app, Json::UInt64 workspace, bool active) {
        auto* button = dock.buttons.at({app, workspace});
        assert(gtk_style_context_has_class(gtk_widget_get_style_context(button), "active")
               == active);
    };
    event(R"({"WorkspacesChanged":{"workspaces":[{"id":1,"idx":1},{"id":2,"idx":2}]}})");
    event(R"({"WindowsChanged":{"windows":[
        {"id":7,"app_id":"kitty","workspace_id":1,"is_focused":true},
        {"id":8,"app_id":"kitty","workspace_id":2,"is_focused":false}
    ]}})");
    // Captured niri tray-reopen sequence: no WindowFocusChanged follows the
    // focused WindowOpenedOrChanged. The old terminal must lose its highlight.
    event(R"({"WindowOpenedOrChanged":{"window":
        {"id":13,"app_id":"amp-desktop","workspace_id":1,"is_focused":true}
    }})");
    assert(dock.buttons.size() == 3);
    check("kitty", 1, false);
    check("amp-desktop", 1, true);
    check("kitty", 2, false);

    auto* hovered = dock.buttons.at({"kitty", 2});
    GtkRequisition rest, hover;
    int dock_height = 0;
    gtk_widget_get_preferred_size(hovered, nullptr, &rest);
    gtk_widget_set_state_flags(hovered, GTK_STATE_FLAG_PRELIGHT, FALSE);
    gtk_widget_get_preferred_size(hovered, nullptr, &hover);
    gtk_widget_get_preferred_height(dock.box, nullptr, &dock_height);
    assert(dock_height == 64);
    assert(rest.height == 56 && hover.height == rest.height && hover.width == rest.width);
    check("amp-desktop", 1, true);
    check("kitty", 2, false);

    // An unrelated window's metadata update must not steal focus.
    event(R"({"WindowOpenedOrChanged":{"window":
        {"id":8,"app_id":"kitty","workspace_id":2,"is_focused":false,"title":"Renamed"}
    }})");
    check("amp-desktop", 1, true);
    event(R"({"WindowFocusChanged":{"id":7}})");
    check("kitty", 1, true);
    check("amp-desktop", 1, false);
    check("kitty", 2, false);
    event(R"({"WindowFocusChanged":{"id":null}})");
    check("kitty", 1, false);
    check("amp-desktop", 1, false);
    check("kitty", 2, false);
    std::cout << "Dock tray-open, ordinary focus, no-focus, workspace grouping, and stable hover slots passed\n";
}
