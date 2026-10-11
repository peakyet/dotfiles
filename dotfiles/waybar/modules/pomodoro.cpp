// Waybar CFFI ABI v2. Persistent timer with the existing Python state format.
#include <gtk/gtk.h>
#include <json/json.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

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

namespace {

int duration(const std::string& phase) {
    return phase == "focus" ? 25 * 60 : phase == "break" ? 5 * 60 : 15 * 60;
}

Json::Value initial_state() {
    Json::Value state;
    state["phase"] = "focus";
    state["completed"] = 0;
    state["remaining"] = duration("focus");
    state["deadline"] = Json::Value();
    return state;
}

double now() {
    timespec time{};
    clock_gettime(CLOCK_BOOTTIME, &time); // Includes suspend, ignores wall-clock changes.
    return time.tv_sec + time.tv_nsec / 1e9;
}

} // namespace

struct Pomodoro {
    GtkWidget* button = gtk_button_new();
    GtkWidget* label = gtk_label_new(nullptr);
    Json::Value state = initial_state();
    std::string path;
    int lock = -1;
    GFileMonitor* monitor = nullptr;
    guint timer = 0;

    ~Pomodoro() {
        if (timer) g_source_remove(timer);
        if (monitor) {
            g_file_monitor_cancel(monitor);
            g_object_unref(monitor);
        }
        if (lock >= 0) close(lock);
        gtk_widget_destroy(button);
    }

    void start() {
        const char* runtime = g_getenv("XDG_RUNTIME_DIR");
        std::string directory = (runtime ? std::string(runtime) :
            "/run/user/" + std::to_string(getuid())) + "/waybar-pomodoro";
        path = directory + "/state.json";
        if (g_mkdir_with_parents(directory.c_str(), 0700) != 0 ||
            (lock = open((directory + "/lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600)) < 0) {
            g_warning("Pomodoro: %s", std::strerror(errno));
            gtk_widget_set_sensitive(button, FALSE);
            render();
            return;
        }
        GFile* file = g_file_new_for_path(directory.c_str());
        GError* error = nullptr;
        monitor = g_file_monitor_directory(file, G_FILE_MONITOR_NONE, nullptr, &error);
        g_object_unref(file);
        if (monitor) {
            g_signal_connect(monitor, "changed", G_CALLBACK(+[](GFileMonitor*, GFile* file,
                             GFile*, GFileMonitorEvent, gpointer data) {
                gchar* name = g_file_get_basename(file);
                bool changed = g_strcmp0(name, "state.json") == 0;
                g_free(name);
                if (changed) static_cast<Pomodoro*>(data)->refresh();
            }), this);
        } else {
            g_warning("Pomodoro: %s", error->message);
            g_error_free(error);
        }
        refresh();
    }

    // Serialize clicks and expiry across bars/processes. Only the process that
    // commits a phase transition sends its notification; normal ticks do no I/O.
    void refresh(const char* action = "status") {
        if (lock < 0) return;
        if (flock(lock, LOCK_EX) != 0) {
            g_warning("Pomodoro: %s", std::strerror(errno));
            return;
        }
        Json::Value next = initial_state();
        std::ifstream input(path);
        if (input) {
            Json::Reader reader;
            if (!reader.parse(input, next)) {
                flock(lock, LOCK_UN);
                g_warning("Pomodoro: invalid state in %s", path.c_str());
                return;
            }
        }
        bool changed = false;
        std::string title, body;
        double time = now();
        if (!next["deadline"].isNull()) {
            next["remaining"] = std::max(0.0, next["deadline"].asDouble() - time);
            if (next["remaining"].asDouble() == 0) {
                if (next["phase"].asString() == "focus") {
                    int completed = next["completed"].asInt() + 1;
                    next["completed"] = completed;
                    next["phase"] = completed % 4 == 0 ? "long-break" : "break";
                    title = "Focus complete";
                    body = "Take a " + std::to_string(duration(next["phase"].asString()) / 60) +
                           "-minute break. Click the timer to start it.";
                } else {
                    next["phase"] = "focus";
                    title = "Break complete";
                    body = "Ready for a 25-minute focus session. Click the timer to start it.";
                }
                next["remaining"] = duration(next["phase"].asString());
                next["deadline"] = Json::Value();
                changed = true;
            }
        }
        if (std::strcmp(action, "reset") == 0) {
            next = initial_state();
            title.clear();
            changed = true;
        } else if (std::strcmp(action, "toggle") == 0) {
            next["deadline"] = next["deadline"].isNull() ?
                Json::Value(time + next["remaining"].asDouble()) : Json::Value();
            changed = true;
        }
        if (changed) {
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "";
            std::string json = Json::writeString(writer, next);
            GError* error = nullptr;
            if (!g_file_set_contents_full(path.c_str(), json.c_str(), json.size(),
                                         G_FILE_SET_CONTENTS_CONSISTENT, 0600, &error)) {
                flock(lock, LOCK_UN);
                g_warning("Pomodoro: %s", error->message);
                g_error_free(error);
                return;
            }
        }
        state = std::move(next);
        flock(lock, LOCK_UN);
        render();
        if (!title.empty()) {
            const char* argv[] = {"notify-send", "-a", "Pomodoro", "-i", "appointment-soon",
                                  title.c_str(), body.c_str(), nullptr};
            GError* error = nullptr;
            if (!g_spawn_async(nullptr, const_cast<char**>(argv), nullptr, G_SPAWN_SEARCH_PATH,
                               nullptr, nullptr, nullptr, &error)) {
                g_warning("Pomodoro: %s", error->message);
                g_error_free(error);
            }
        }
    }

    void render() {
        bool running = !state["deadline"].isNull();
        std::string phase = state["phase"].asString();
        double remaining = running ? std::max(0.0, state["deadline"].asDouble() - now()) :
                                     state["remaining"].asDouble();
        const char* status = running ? "running" : remaining == duration(phase) ? "ready" : "paused";
        const char* icon = std::strcmp(status, "paused") == 0 ? "" : phase == "focus" ? "" : "";
        const char* colour = running ? (phase == "focus" ? "#FF453A" : "#0A84FF") : "#6E6E73";
        std::string text = std::string("<span foreground=\"") + colour + "\">" + icon + "</span>";
        if (std::strcmp(status, "ready") != 0) {
            int seconds = static_cast<int>(std::ceil(remaining));
            gchar* countdown = g_strdup_printf("  %02d:%02d", seconds / 60, seconds % 60);
            text += countdown;
            g_free(countdown);
        }
        gtk_label_set_markup(GTK_LABEL(label), text.c_str());
        auto* style = gtk_widget_get_style_context(button);
        for (const char* name : {"focus", "break", "long-break", "running", "ready", "paused"})
            gtk_style_context_remove_class(style, name);
        gtk_style_context_add_class(style, phase.c_str());
        gtk_style_context_add_class(style, status);
        const char* phase_label = phase == "focus" ? "Focus" : phase == "break" ? "Short break" : "Long break";
        const char* status_label = running ? "Running" : std::strcmp(status, "ready") == 0 ? "Ready" : "Paused";
        std::string tooltip = std::string("Pomodoro · ") + phase_label + " · " + status_label +
            "\nCompleted focus sessions: " + std::to_string(state["completed"].asInt()) +
            "\n\nLeft click: start / pause / resume\nRight click: reset to 25:00\n"
            "25 min focus · 5 min break · 15 min break after 4 sessions\n"
            "Each new session waits for your click.";
        gtk_widget_set_tooltip_text(button, tooltip.c_str());
        if (running && !timer) {
            timer = g_timeout_add(1000, [](gpointer data) -> gboolean {
                auto* pomodoro = static_cast<Pomodoro*>(data);
                pomodoro->timer = 0;
                if (now() >= pomodoro->state["deadline"].asDouble()) pomodoro->refresh();
                else pomodoro->render();
                return G_SOURCE_REMOVE;
            }, this);
        } else if (!running && timer) {
            g_source_remove(timer);
            timer = 0;
        }
    }
};

extern "C" {
void* wbcffi_init(const wbcffi_init_info* info, const wbcffi_config_entry*, size_t) {
    auto* pomodoro = new Pomodoro;
    gtk_widget_set_name(pomodoro->button, "custom-pomodoro");
    gtk_container_add(GTK_CONTAINER(pomodoro->button), pomodoro->label);
    atk_object_set_name(gtk_widget_get_accessible(pomodoro->button), "Pomodoro");
    g_signal_connect(pomodoro->button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        static_cast<Pomodoro*>(data)->refresh("toggle");
    }), pomodoro);
    g_signal_connect(pomodoro->button, "button-press-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer data) -> gboolean {
            if (event->button != 3) return FALSE;
            static_cast<Pomodoro*>(data)->refresh("reset");
            return TRUE;
        }), pomodoro);
    gtk_container_add(info->get_root_widget(info->obj), pomodoro->button);
    gtk_widget_show_all(pomodoro->button);
    pomodoro->start();
    return pomodoro;
}
void wbcffi_deinit(void* instance) { delete static_cast<Pomodoro*>(instance); }
void wbcffi_update(void*) {}
void wbcffi_refresh(void* instance, int) { static_cast<Pomodoro*>(instance)->refresh(); }
void wbcffi_doaction(void* instance, const char* action) {
    if (std::strcmp(action, "toggle") == 0 || std::strcmp(action, "reset") == 0)
        static_cast<Pomodoro*>(instance)->refresh(action);
}
}
