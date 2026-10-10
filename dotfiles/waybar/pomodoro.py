#!/usr/bin/env python3
"""A click-controlled Pomodoro timer for Waybar; Python standard library only."""

import argparse
import fcntl
import json
import math
import os
from pathlib import Path
import subprocess
import time


DURATIONS = {"focus": 25 * 60, "break": 5 * 60, "long-break": 15 * 60}
LABELS = {"focus": "Focus", "break": "Short break", "long-break": "Long break"}


def initial_state():
    return {"phase": "focus", "completed": 0, "remaining": DURATIONS["focus"], "deadline": None}


def update(state, action, now):
    """Advance by elapsed time, then apply a click. New phases wait for a click."""
    notification = None
    if state["deadline"] is not None:
        state["remaining"] = max(0, state["deadline"] - now)
        if state["remaining"] == 0:
            if state["phase"] == "focus":
                state["completed"] += 1
                state["phase"] = "long-break" if state["completed"] % 4 == 0 else "break"
                minutes = DURATIONS[state["phase"]] // 60
                notification = ("Focus complete", f"Take a {minutes}-minute break. Click the timer to start it.")
            else:
                state["phase"] = "focus"
                minutes = DURATIONS["focus"] // 60
                notification = ("Break complete", f"Ready for a {minutes}-minute focus session. Click the timer to start it.")
            state["remaining"] = DURATIONS[state["phase"]]
            state["deadline"] = None

    if action == "reset":
        state.update(initial_state())
        notification = None
    elif action == "toggle":
        if state["deadline"] is None:
            state["deadline"] = now + state["remaining"]
        else:
            state["deadline"] = None
    return notification


def render(state):
    seconds = math.ceil(state["remaining"])
    running = state["deadline"] is not None
    status = "running" if running else "ready" if state["remaining"] == DURATIONS[state["phase"]] else "paused"
    icon = "" if status == "paused" else "" if state["phase"] == "focus" else ""
    color = "#FF453A" if running and state["phase"] == "focus" else "#0A84FF" if running else "#6E6E73"
    label = LABELS[state["phase"]]
    text = f'<span foreground="{color}">{icon}</span>'
    if status != "ready":
        text += f"  {seconds // 60:02d}:{seconds % 60:02d}"
    return {
        "text": text,
        "class": [state["phase"], status],
        "tooltip": (
            f"Pomodoro · {label} · {status.title()}\n"
            f"Completed focus sessions: {state['completed']}\n\n"
            "Left click: start / pause / resume\n"
            "Right click: reset to 25:00\n"
            "25 min focus · 5 min break · 15 min break after 4 sessions\n"
            "Each new session waits for your click."
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", nargs="?", choices=("status", "toggle", "reset"), default="status")
    args = parser.parse_args()

    # Runtime storage survives Waybar reloads, but clears on logout/reboot.
    directory = Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "waybar-pomodoro"
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    path = directory / "state.json"
    with (directory / "lock").open("w") as lock:
        # Serialize polls and clicks, including Waybar instances on other monitors.
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = json.loads(path.read_text()) if path.exists() else initial_state()
        # BOOTTIME includes suspend and is unaffected by wall-clock corrections.
        notification = update(state, args.action, time.clock_gettime(time.CLOCK_BOOTTIME))
        temporary = directory / "state.tmp"
        temporary.write_text(json.dumps(state))
        temporary.replace(path)
        output = render(state)

    if notification:
        try:
            subprocess.run(
                ["notify-send", "-a", "Pomodoro", "-i", "appointment-soon", *notification],
                check=False, timeout=5,
            )
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass
    print(json.dumps(output))


if __name__ == "__main__":
    main()
