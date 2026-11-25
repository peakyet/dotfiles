// Waybar CFFI ABI v2: https://github.com/Alexays/Waybar/tree/0.15.0/resources/custom_modules/cffi_example
//
// Niri's workspaces in a translucent segmented control. The focused tile shows
// its roman numeral; the other tiles are filled when occupied, outlined when
// empty, and softly dimmed with distance. Niri's event stream drives repainting.
#include <gtk/gtk.h>
#include <gio/gfiledescriptorbased.h>
#include <pango/pangocairo.h>
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

constexpr double kActiveWidth = 24;
constexpr double kActiveHeight = 18;
constexpr double kInactiveWidth = 12;
constexpr double kInactiveHeight = 9;
constexpr double kGap = 4;
constexpr double kTrackPadding = 3;
constexpr double kTrackHeight = 22;
constexpr int kBandHeight = 24;
constexpr double kNumeralSize = 11;
constexpr double kNumeralPadding = 6;

const GdkRGBA kFallbackPip = {0.24, 0.24, 0.26, 0.78};
const GdkRGBA kFallbackUrgent = {1.0, 0.27, 0.23, 1.0};
const GdkRGBA kFallbackNumeral = {0.11, 0.11, 0.12, 1.0};
const GdkRGBA kFallbackTrack = {0.96, 0.96, 0.97, 0.46};
const GdkRGBA kFallbackSelection = {1.0, 1.0, 1.0, 0.92};

// Roman numerals for a workspace index. niri numbers workspaces from 1, so the
// table stops at I; anything larger just gets more glyphs.
std::string roman(int number) {
    static const std::pair<int, const char*> kTable[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"},
        {50, "L"},   {40, "XL"},  {10, "X"},  {9, "IX"},   {5, "V"},   {4, "IV"},
        {1, "I"},
    };
    if (number < 1) return std::to_string(number);
    std::string numeral;
    for (const auto& [value, glyph] : kTable)
        for (; number >= value; number -= value) numeral += glyph;
    return numeral;
}

// Rounded rectangles for the track and its individual tiles.
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
    std::string numeral;                 // the focused workspace's index, in roman numerals
    mutable PangoLayout* numeral_layout = nullptr;

    struct Pip {
        Json::UInt64 id;
        double x, y, width, height;
        double opacity;
        bool urgent;
        bool occupied;
        bool labelled; // the pip the numeral belongs to
    };

    ~Pips() {
        if (watch) g_source_remove(watch);
        if (retry) g_source_remove(retry);
        if (numeral_layout) g_object_unref(numeral_layout);
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

    void set_focused(const Json::Value& workspace) {
        focused = workspace["idx"].asInt();
        numeral = roman(focused);
    }

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
        } else if (event.isMember("WorkspaceActiveWindowChanged")) {
            const auto& change = event["WorkspaceActiveWindowChanged"];
            Json::Value* workspace = find(change["workspace_id"].asUInt64());
            if (!workspace) return false;
            (*workspace)["active_window_id"] = change["active_window_id"];
        } else {
            return false;
        }
        return true;
    }

    double width_of(const Json::Value& workspace) const {
        return workspace["idx"].asInt() == focused ? focused_numeral_width() : kInactiveWidth;
    }

    // The focused pill keeps its usual width unless the numeral needs more.
    double focused_numeral_width() const {
        PangoRectangle ink;
        numeral_ink(ink);
        return std::max(kActiveWidth, ink.width + 2 * kNumeralPadding);
    }

    // The layout the numeral is drawn and measured with, with the text applied.
    PangoLayout* text_layout() const {
        if (!numeral_layout) {
            numeral_layout = pango_layout_new(gtk_widget_get_pango_context(area));
            PangoAttrList* attributes = pango_attr_list_new();
            // Absolute size, so the numeral ignores the bar's 13px body font while
            // still taking its family from the theme.
            pango_attr_list_insert(attributes,
                pango_attr_size_new_absolute(kNumeralSize * PANGO_SCALE));
            pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            pango_layout_set_attributes(numeral_layout, attributes);
            pango_attr_list_unref(attributes);
        }
        pango_layout_set_text(numeral_layout, numeral.c_str(), -1);
        return numeral_layout;
    }

    // The numeral's bounding box in pixels, empty when there is no numeral yet.
    void numeral_ink(PangoRectangle& ink) const {
        ink = PangoRectangle{0, 0, 0, 0};
        if (numeral.empty()) return;
        pango_layout_get_pixel_extents(text_layout(), &ink, nullptr);
    }

    // Lays the pips out left to right, centred in a band of the given size.
    std::vector<Pip> layout(double width, double band) const {
        std::vector<Pip> pips;
        double total = 0;
        for (const auto& workspace : workspaces) {
            Pip pip{};
            pip.id = workspace["id"].asUInt64();
            const int distance = std::abs(workspace["idx"].asInt() - focused);
            pip.opacity = 1 - 0.06 * std::min(distance, 4);
            pip.width = width_of(workspace);
            pip.urgent = workspace["is_urgent"].asBool();
            pip.occupied = !workspace["active_window_id"].isNull();
            pip.labelled = distance == 0;
            pip.height = pip.labelled ? kActiveHeight : kInactiveHeight;
            pip.y = (band - pip.height) / 2;
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

    void render() {
        gtk_widget_set_size_request(area, natural_width(), kBandHeight);
        gtk_widget_queue_draw(area);
    }

    int natural_width() const {
        double total = 0;
        for (const auto& workspace : workspaces) total += width_of(workspace) + kGap;
        if (!workspaces.empty()) total += 2 * kTrackPadding - kGap;
        return static_cast<int>(std::ceil(total));
    }

    // The focused workspace's index, in roman numerals, centred in its pip.
    void draw_numeral(cairo_t* cr, const Pip& pip, const GdkRGBA& colour) const {
        if (!pip.labelled) return;
        PangoRectangle ink;
        numeral_ink(ink);
        if (ink.width <= 0 || ink.height <= 0) return;
        gdk_cairo_set_source_rgba(cr, &colour);
        // Centre the ink box, not the layout box: a numeral has no descender.
        cairo_move_to(cr, std::round(pip.x + (pip.width - ink.width) / 2.0 - ink.x),
                          std::round(pip.y + (pip.height - ink.height) / 2.0 - ink.y));
        pango_cairo_show_layout(cr, text_layout());
    }

    static void on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
        auto* pips = static_cast<Pips*>(data);
        auto* style = gtk_widget_get_style_context(widget);
        GdkRGBA base = kFallbackPip, urgent = kFallbackUrgent, text = kFallbackNumeral;
        GdkRGBA track = kFallbackTrack, selection = kFallbackSelection;
        gtk_style_context_lookup_color(style, "mac_ws_pip", &base);
        gtk_style_context_lookup_color(style, "mac_ws_urgent", &urgent);
        gtk_style_context_lookup_color(style, "mac_text", &text);
        gtk_style_context_lookup_color(style, "mac_ws_track", &track);
        gtk_style_context_lookup_color(style, "mac_ws_selection", &selection);

        const auto tiles = pips->layout();
        if (tiles.empty()) return;
        const double x = tiles.front().x - kTrackPadding;
        const double y = (gtk_widget_get_allocated_height(widget) - kTrackHeight) / 2;
        const double width = tiles.back().x + tiles.back().width + kTrackPadding - x;
        cairo_set_line_width(cr, 1);
        rounded_pip(cr, x + 0.5, y + 0.5, width - 1, kTrackHeight - 1, 7);
        gdk_cairo_set_source_rgba(cr, &track);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.45);
        cairo_stroke(cr);

        for (const auto& pip : tiles) {
            if (pip.labelled) {
                // A one-pixel shadow lifts the selected tile off the glass track.
                rounded_pip(cr, pip.x, pip.y + 1, pip.width, pip.height, 5);
                cairo_set_source_rgba(cr, 0, 0, 0, 0.15);
                cairo_fill(cr);
                rounded_pip(cr, pip.x + 0.5, pip.y + 0.5, pip.width - 1, pip.height - 1, 5);
                gdk_cairo_set_source_rgba(cr, &selection);
                cairo_fill_preserve(cr);
                if (pip.urgent) gdk_cairo_set_source_rgba(cr, &urgent);
                else cairo_set_source_rgba(cr, 1, 1, 1, 0.75);
                cairo_stroke(cr);
                pips->draw_numeral(cr, pip, text);
                continue;
            }
            GdkRGBA colour = pip.urgent ? urgent : base;
            if (!pip.urgent) colour.alpha *= pip.opacity;
            gdk_cairo_set_source_rgba(cr, &colour);
            rounded_pip(cr, pip.x + 0.5, pip.y + 0.5, pip.width - 1, pip.height - 1, 2);
            if (pip.occupied) cairo_fill(cr);
            else cairo_stroke(cr);
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
