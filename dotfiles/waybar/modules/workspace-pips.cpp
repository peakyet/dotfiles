// Waybar CFFI ABI v2: https://github.com/Alexays/Waybar/tree/0.15.0/resources/custom_modules/cffi_example
//
// Workspace indicator for niri: one rounded pip per workspace, sized and faded
// by its distance from the focused workspace, so the focused workspace is the
// widest, strongest pip and its neighbours trail off into small, faint ones.
//
// This is a module of its own because GTK CSS cannot express it: CSS can only
// style waybar's workspace buttons by state class (active/focused/empty/urgent),
// never by where a workspace sits relative to the focused one, and the falloff
// has to be recomputed whenever focus moves. The widget follows niri's event
// stream, so it repaints without polling.
//
// Geometry and the falloff shape are the constants below; the pip colour comes
// from `mac_ws_pip` (and `mac_ws_urgent` for urgent workspaces) in style.css, so
// the palette stays with the rest of the theme. Alpha alone fades with distance,
// so pointing `mac_ws_pip` at any colour gives that colour a falloff.
#include <gtk/gtk.h>
#include <gio/gfiledescriptorbased.h>
#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
struct wbcffi_config_entry {
    const char* key;
    const char* value;
};
}

namespace {

constexpr double kActiveWidth = 24;  // keep the focused workspace at its original size
constexpr double kActiveHeight = 12;
constexpr double kFaintWidth = 2;    // the other workspaces: slim bars
constexpr double kFaintHeight = 4;
constexpr double kGap = 6;           // between pips
constexpr double kFaintAlpha = 0.20; // how far the fade goes, relative to the theme colour
constexpr int kSteps = 4;            // distance at which the height and alpha falloff bottoms out
constexpr int kWidthSteps = 4;       // ... and the width falloff
constexpr double kWidthCurve = 8;    // keep neighbours slim while their height tapers gradually
constexpr int kBandHeight = 24;      // height the pips are centred in

const GdkRGBA kFallbackPip = {0.0, 0.0, 0.0, 0.7};
const GdkRGBA kFallbackUrgent = {1.0, 0.27, 0.23, 1.0};

// The same softly rounded rectangle outline for focus and the surrounding bars.
void rounded_pip(cairo_t* cr, double x, double y, double width, double height,
                 double radius) {
    cairo_new_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -G_PI / 2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, G_PI / 2);
    cairo_arc(cr, x + radius, y + height - radius, radius, G_PI / 2, G_PI);
    cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
}

} // namespace

struct Pips {
    GtkWidget* area = gtk_drawing_area_new();
    GSubprocess* stream = nullptr;
    GIOChannel* channel = nullptr;
    guint watch = 0, retry = 0;
    std::vector<Json::Value> workspaces; // sorted by index
    int focused = 0;                     // index of the focused workspace

    struct Pip {
        Json::UInt64 id;
        double x, y, width, height;
        double fade; // proximity to the focused workspace: 1 there, 0 at the far end
        bool urgent;
    };

    ~Pips() {
        if (watch) g_source_remove(watch);
        if (retry) g_source_remove(retry);
        stop_stream();
        gtk_widget_destroy(area);
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
            g_warning("Workspace pips: %s", error->message);
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
                auto* pips = static_cast<Pips*>(data);
                bool changed = false;
                GIOStatus status;
                gchar* line = nullptr;
                while ((status = g_io_channel_read_line(channel, &line, nullptr,
                                                       nullptr, nullptr)) == G_IO_STATUS_NORMAL) {
                    Json::Value event;
                    Json::Reader reader;
                    if (reader.parse(line, event)) changed |= pips->handle_event(event);
                    g_free(line);
                }
                if (changed) pips->render();
                if ((condition & (G_IO_HUP | G_IO_ERR)) || status == G_IO_STATUS_EOF ||
                    status == G_IO_STATUS_ERROR) {
                    pips->watch = 0;
                    pips->stop_stream();
                    pips->reconnect();
                    return G_SOURCE_REMOVE;
                }
                return G_SOURCE_CONTINUE;
            }, this);
    }

    void reconnect() {
        retry = g_timeout_add_seconds(2, [](gpointer data) -> gboolean {
            auto* pips = static_cast<Pips*>(data);
            pips->retry = 0;
            pips->connect_stream();
            return G_SOURCE_REMOVE;
        }, this);
    }

    void adopt(std::vector<Json::Value> list) {
        workspaces = std::move(list);
        std::sort(workspaces.begin(), workspaces.end(),
                  [](const Json::Value& a, const Json::Value& b) {
                      return a["idx"].asInt() < b["idx"].asInt();
                  });
        for (const auto& workspace : workspaces) {
            if (workspace["is_focused"].asBool()) return set_focused(workspace);
        }
        for (const auto& workspace : workspaces) {
            if (workspace["is_active"].asBool()) return set_focused(workspace);
        }
    }

    void set_focused(const Json::Value& workspace) { focused = workspace["idx"].asInt(); }

    Json::Value* find(Json::UInt64 id) {
        for (auto& workspace : workspaces)
            if (workspace["id"].asUInt64() == id) return &workspace;
        return nullptr;
    }

    bool handle_event(const Json::Value& event) {
        if (event.isMember("WorkspacesChanged")) {
            std::vector<Json::Value> list;
            for (const auto& workspace : event["WorkspacesChanged"]["workspaces"])
                list.push_back(workspace);
            adopt(std::move(list));
        } else if (event.isMember("WorkspaceActivated")) {
            const auto& activated = event["WorkspaceActivated"];
            if (!activated["focused"].asBool()) return false;
            Json::Value* workspace = find(activated["id"].asUInt64());
            if (!workspace) return false;
            set_focused(*workspace);
        } else if (event.isMember("WorkspaceUrgencyChanged")) {
            const auto& change = event["WorkspaceUrgencyChanged"];
            Json::Value* workspace = find(change["id"].asUInt64());
            if (!workspace) return false;
            (*workspace)["is_urgent"] = change["urgent"];
        } else {
            return false;
        }
        return true;
    }

    // How close a workspace is to the focused one: 1 at the focused workspace,
    // 0 once it is `steps` or more away.
    double proximity_of(const Json::Value& workspace, int steps, double curve = 1) const {
        int distance = std::abs(workspace["idx"].asInt() - focused);
        if (steps <= 0) return distance == 0 ? 1 : 0;
        double proximity = 1 - std::min(distance, steps) / static_cast<double>(steps);
        return curve == 1 ? proximity : std::pow(proximity, curve);
    }

    double width_of(const Json::Value& workspace) const {
        return kFaintWidth + (kActiveWidth - kFaintWidth) *
                             proximity_of(workspace, kWidthSteps, kWidthCurve);
    }

    // Lays the pips out left to right, centred in a band of the given size.
    std::vector<Pip> layout(double width, double band) const {
        std::vector<Pip> pips;
        double total = 0;
        for (const auto& workspace : workspaces) {
            Pip pip{};
            pip.id = workspace["id"].asUInt64();
            pip.fade = proximity_of(workspace, kSteps);
            pip.width = width_of(workspace);
            pip.height = kFaintHeight + (kActiveHeight - kFaintHeight) * pip.fade;
            pip.y = (band - pip.height) / 2;
            pip.urgent = workspace["is_urgent"].asBool();
            pips.push_back(pip);
            total += pip.width + kGap;
        }
        if (!pips.empty()) total -= kGap;
        double x = (width - total) / 2;
        for (auto& pip : pips) {
            pip.x = x;
            x += pip.width + kGap;
        }
        return pips;
    }

    // The current band, laid out in the widget's allocation.
    std::vector<Pip> layout() const {
        return layout(gtk_widget_get_allocated_width(area),
                      gtk_widget_get_allocated_height(area));
    }

    // A pip's colour: the theme's pip colour, or the urgent one, at the alpha
    // the pip's distance from the focused workspace has earned it.
    static GdkRGBA colour_of(const Pip& pip, const GdkRGBA& base, const GdkRGBA& urgent) {
        GdkRGBA colour = pip.urgent ? urgent : base;
        colour.alpha *= kFaintAlpha + (1 - kFaintAlpha) * pip.fade;
        return colour;
    }

    void render() {
        gtk_widget_set_size_request(area, natural_width(), kBandHeight);
        gtk_widget_queue_draw(area);
    }

    int natural_width() const {
        double total = 0;
        for (const auto& workspace : workspaces) total += width_of(workspace) + kGap;
        if (!workspaces.empty()) total -= kGap;
        return static_cast<int>(std::ceil(total));
    }

    static void on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
        auto* pips = static_cast<Pips*>(data);
        auto* style = gtk_widget_get_style_context(widget);
        GdkRGBA base = kFallbackPip, urgent = kFallbackUrgent;
        gtk_style_context_lookup_color(style, "mac_ws_pip", &base);
        gtk_style_context_lookup_color(style, "mac_ws_urgent", &urgent);
        for (const auto& pip : pips->layout()) {
            GdkRGBA colour = colour_of(pip, base, urgent);
            gdk_cairo_set_source_rgba(cr, &colour);
            double radius = std::min(pip.width, pip.height) * 0.35;
            rounded_pip(cr, pip.x, pip.y, pip.width, pip.height, radius);
            cairo_fill(cr);
        }
    }

    static void focus(int index) {
        std::string number = std::to_string(index);
        const char* argv[] = {"niri", "msg", "action", "focus-workspace", number.c_str(), nullptr};
        GError* error = nullptr;
        if (!g_spawn_async(nullptr, const_cast<char**>(argv), nullptr, G_SPAWN_SEARCH_PATH,
                           nullptr, nullptr, nullptr, &error)) {
            g_warning("Workspace pips: %s", error->message);
            g_error_free(error);
        }
    }

    static gboolean on_button(GtkWidget*, GdkEventButton* event, gpointer data) {
        if (event->button != 1) return FALSE;
        auto* pips = static_cast<Pips*>(data);
        for (const auto& pip : pips->layout()) {
            if (event->x >= pip.x && event->x < pip.x + pip.width) {
                for (const auto& workspace : pips->workspaces)
                    if (workspace["id"].asUInt64() == pip.id) focus(workspace["idx"].asInt());
                return TRUE;
            }
        }
        return FALSE;
    }
};

extern "C" {
void* wbcffi_init(const wbcffi_init_info* info, const wbcffi_config_entry*, size_t) {
    auto* pips = new Pips;
    // Named "workspaces" so style.css styles it with the same selector the
    // stock niri/workspaces module used.
    gtk_widget_set_name(pips->area, "workspaces");
    gtk_widget_set_size_request(pips->area, pips->natural_width(), kBandHeight);
    gtk_widget_set_events(pips->area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(pips->area, "draw", G_CALLBACK(Pips::on_draw), pips);
    g_signal_connect(pips->area, "button-press-event", G_CALLBACK(Pips::on_button), pips);
    gtk_container_add(info->get_root_widget(info->obj), pips->area);
    gtk_widget_show(pips->area);
    pips->connect_stream();
    return pips;
}
void wbcffi_deinit(void* instance) { delete static_cast<Pips*>(instance); }
void wbcffi_update(void*) {}
void wbcffi_refresh(void*, int) {}
void wbcffi_doaction(void*, const char*) {}
}
