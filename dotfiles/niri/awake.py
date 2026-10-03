#!/usr/bin/env python3
"""Keep the session awake while a long-running command runs.

The screen lock and the DPMS power-off both come from swayidle, and niri stops
delivering swayidle's idle notifications while any client holds
org.freedesktop.ScreenSaver.Inhibit on the session bus. Holding that inhibit is
therefore enough to keep the screen unlocked and the monitors on for as long as
this process lives; the lock timers restart once it is released. niri also drops
the inhibit on its own when the holding process dies, so a crash cannot leave
the session permanently awake.

Usage:
    awake.py [--] command [args...]   hold while the command runs
    awake.py                          hold until interrupted (Ctrl-C)

The `awake` shell function in zshrc/bashrc wraps this together with
systemd-inhibit, which additionally blocks sleep and lid-close suspend.
"""

import signal
import subprocess
import sys
import time


class IdleInhibitor:
    """Holds org.freedesktop.ScreenSaver.Inhibit for this process's lifetime."""

    BUS_NAME = "org.freedesktop.ScreenSaver"
    OBJECT_PATH = "/org/freedesktop/ScreenSaver"
    INTERFACE = "org.freedesktop.ScreenSaver"
    APP_NAME = "awake"
    REASON = "a long-running command is still working"

    def __init__(self):
        self._gio = None
        self._glib = None
        self._bus = None
        self._cookie = None

    def hold(self):
        """Acquire the inhibitor, or warn and continue without one."""
        try:
            import gi

            gi.require_version("Gio", "2.0")
            from gi.repository import Gio, GLib
        except (ImportError, ValueError) as err:
            print(f"awake: D-Bus bindings unavailable ({err}); not inhibiting idle", file=sys.stderr)
            return
        self._gio, self._glib = Gio, GLib
        try:
            self._bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
            reply = self._bus.call_sync(
                self.BUS_NAME,
                self.OBJECT_PATH,
                self.INTERFACE,
                "Inhibit",
                GLib.Variant("(ss)", (self.APP_NAME, self.REASON)),
                GLib.VariantType.new("(u)"),
                Gio.DBusCallFlags.NONE,
                -1,
                None,
            )
        except Exception as err:  # no session bus, or no screensaver service
            self._bus = None
            print(f"awake: idle inhibit unavailable ({err}); not inhibiting idle", file=sys.stderr)
            return
        self._cookie = reply.unpack()[0]

    def release(self):
        if self._bus is None or self._cookie is None:
            return
        try:
            self._bus.call_sync(
                self.BUS_NAME,
                self.OBJECT_PATH,
                self.INTERFACE,
                "UnInhibit",
                self._glib.Variant("(u)", (self._cookie,)),
                None,
                self._gio.DBusCallFlags.NONE,
                -1,
                None,
            )
        except Exception:
            # Dropping the bus connection releases the inhibit too, so a failed
            # UnInhibit is not worth reporting.
            pass
        self._bus = None
        self._cookie = None


def main(argv):
    command = argv[1:]
    if command[:1] == ["--"]:
        command = command[1:]

    inhibitor = IdleInhibitor()
    inhibitor.hold()
    try:
        if not command:
            print("awake: holding; press Ctrl-C to release", file=sys.stderr)
            while True:
                time.sleep(3600)

        child = subprocess.Popen(command)
        # Ctrl-C in the terminal should stop the wrapped command, not this holder.
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(sig, lambda num, _frame: child.send_signal(num))
        return child.wait()
    except KeyboardInterrupt:
        return 130
    finally:
        inhibitor.release()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
