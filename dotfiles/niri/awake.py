#!/usr/bin/env python3
"""Keep the session awake while a long-running command runs.

The screen lock and the DPMS power-off both come from swayidle, and niri stops
delivering swayidle's idle notifications while any client holds
org.freedesktop.ScreenSaver.Inhibit on the session bus. Holding that inhibit is
therefore enough to keep the screen unlocked for as long as this process lives;
the lock timers restart once it is released. niri also drops the inhibit on its
own when the holding process dies, so a crash cannot leave the session
permanently awake.

Muting swayidle mutes its DPMS timeout along with its lock timeout, which would
leave the monitors lit for the whole run, so this process watches the input
devices itself and runs `niri msg action power-off-monitors` once the user has
been idle for as long as swayidle would have waited. The session stays
unlocked: niri turns the monitors back on by itself on the next input event, so
walking back to the machine shows the command's output, not a lock screen.

Usage:
    awake.py [--] command [args...]   hold while the command runs
    awake.py                          hold until interrupted (Ctrl-C)

AWAKE_MONITOR_TIMEOUT overrides the idle seconds before the monitors go off.

The `awake` shell function in zshrc/bashrc wraps this together with
systemd-inhibit, which additionally blocks sleep and lid-close suspend.
"""

import glob
import os
import select
import signal
import struct
import subprocess
import sys
import threading
import time

# Same idle timeout as the `timeout 601` in swayidle.service, which this process
# has to reproduce while it holds the inhibit.
MONITOR_OFF_TIMEOUT = 601
# struct input_event: two timeval fields, then type, code and value.
INPUT_EVENT = struct.Struct("qqHHi")
# EV_KEY, EV_REL and EV_ABS are the types that mean the user did something.
# Sync events are filtered because they only accompany real ones, and switch
# events because a chatty sensor should not keep the monitors lit.
ACTIVITY_TYPES = frozenset((1, 2, 3))


def monitor_timeout():
    """Idle seconds before the monitors go off, from AWAKE_MONITOR_TIMEOUT."""
    raw = os.environ.get("AWAKE_MONITOR_TIMEOUT")
    if not raw:
        return MONITOR_OFF_TIMEOUT
    try:
        return float(raw)
    except ValueError:
        print(f"awake: ignoring invalid AWAKE_MONITOR_TIMEOUT={raw!r}", file=sys.stderr)
        return MONITOR_OFF_TIMEOUT


class MonitorBlanker:
    """Powers the monitors off after `timeout` seconds without user input.

    Reading /dev/input/event* needs no more than membership in the `input`
    group. Devices that cannot be opened are skipped, and without any of them
    the monitors simply stay on.
    """

    DEVICE_GLOB = "/dev/input/event*"

    def __init__(self, timeout, devices=None, command=None):
        self._timeout = timeout
        self._devices = devices
        self._command = command or ["niri", "msg", "action", "power-off-monitors"]
        self._fds = {}
        self._wake_r, self._wake_w = os.pipe()
        self._thread = None

    def start(self):
        paths = sorted(glob.glob(self.DEVICE_GLOB)) if self._devices is None else self._devices
        for path in paths:
            try:
                self._fds[os.open(path, os.O_RDONLY | os.O_NONBLOCK)] = path
            except OSError as err:
                print(f"awake: not watching {path}: {err}", file=sys.stderr)
        if not self._fds:
            print("awake: no input device to watch; leaving the monitors on", file=sys.stderr)
            return
        self._thread = threading.Thread(target=self._watch, daemon=True)
        self._thread.start()

    def stop(self):
        # The pipe wakes the select below so the thread can exit right away
        # instead of waiting out the rest of the timeout.
        os.write(self._wake_w, b"x")
        if self._thread is not None:
            self._thread.join()
        os.close(self._wake_r)
        os.close(self._wake_w)

    def _watch(self):
        last_activity = time.monotonic()
        blanked = False
        try:
            while True:
                # Once the monitors are off, nothing is left to do until the
                # user comes back, so wait for input without a deadline.
                if blanked:
                    deadline = None
                else:
                    deadline = max(self._timeout - (time.monotonic() - last_activity), 0)
                ready, _, _ = select.select(list(self._fds) + [self._wake_r], [], [], deadline)
                if self._wake_r in ready:
                    return
                if self._activity(ready):
                    last_activity = time.monotonic()
                    blanked = False
                elif not blanked and time.monotonic() - last_activity >= self._timeout:
                    self._power_off()
                    blanked = True
        finally:
            for fd in self._fds:
                os.close(fd)
            self._fds = {}

    def _activity(self, ready):
        """Read every ready device and report whether real input arrived."""
        active = False
        for fd in ready:
            if fd == self._wake_r:
                continue
            data = self._read(fd)
            if data is None:
                self._drop(fd)
                continue
            for offset in range(0, len(data) - INPUT_EVENT.size + 1, INPUT_EVENT.size):
                if INPUT_EVENT.unpack_from(data, offset)[2] in ACTIVITY_TYPES:
                    active = True
        return active

    def _read(self, fd):
        """Read the events waiting on one device, or None when the device is gone.

        A device that was unplugged keeps reporting readiness, so it has to be
        dropped instead of being read from forever.
        """
        try:
            data = os.read(fd, INPUT_EVENT.size * 64)
        except BlockingIOError:
            # The readiness went stale before we got here; nothing to read.
            return b""
        except OSError as err:
            print(f"awake: dropping {self._fds[fd]}: {err}", file=sys.stderr)
            return None
        if not data:
            print(f"awake: dropping {self._fds[fd]}: device went away", file=sys.stderr)
            return None
        return data

    def _drop(self, fd):
        os.close(fd)
        del self._fds[fd]

    def _power_off(self):
        try:
            subprocess.run(self._command, check=False)
        except OSError as err:
            print(f"awake: {self._command[0]}: {err}; leaving the monitors on", file=sys.stderr)


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
    blanker = MonitorBlanker(monitor_timeout())
    blanker.start()
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
        blanker.stop()
        inhibitor.release()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
