// Run with GTK's Wayland backend; no test windows or compositor actions are created.
#include "../modules/grouped-taskbar.cpp"

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);
    Dock dock;
    auto event = [&](const char* json) {
        Json::Value value;
        Json::Reader reader;
        assert(reader.parse(json, value));
        assert(dock.handle_event(value));
        dock.render();
    };
    // Assert rendered output, not the cached per-window flags. The same app on
    // another workspace must remain separate and outside the neighbour step.
    auto check = [&](const char* app, Json::UInt64 workspace, bool active, int size) {
        auto* button = dock.buttons.at({app, workspace});
        assert(gtk_style_context_has_class(gtk_widget_get_style_context(button), "active")
               == active);
        assert(gtk_image_get_pixel_size(GTK_IMAGE(g_object_get_data(G_OBJECT(button), "icon")))
               == size);
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
    check("kitty", 1, false, 22);
    check("amp-desktop", 1, true, 36);
    check("kitty", 2, false, 22);

    // An unrelated window's metadata update must not steal focus.
    event(R"({"WindowOpenedOrChanged":{"window":
        {"id":8,"app_id":"kitty","workspace_id":2,"is_focused":false,"title":"Renamed"}
    }})");
    check("amp-desktop", 1, true, 36);
    event(R"({"WindowFocusChanged":{"id":7}})");
    check("kitty", 1, true, 36);
    check("amp-desktop", 1, false, 22);
    check("kitty", 2, false, 18);
    event(R"({"WindowFocusChanged":{"id":null}})");
    check("kitty", 1, false, 18);
    check("amp-desktop", 1, false, 18);
    check("kitty", 2, false, 18);
    std::cout << "Dock tray-open, ordinary focus, no-focus, and workspace grouping passed\n";
}
