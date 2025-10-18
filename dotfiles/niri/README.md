This is a Brief Intro for Niri

# Overview

To set up niri, you should run `niri-session` in tty.

## Command Keybind

| keybind                   |                                          usage |
| -                         |                                              - |
| Mod + Shift + /           |                              open hotkeys list |

The general system is: if a hotkey switches somewhere, then adding `Ctrl` will move the focused window or column there.

## systemd setup

This provides the necessary systemd integration to run programs like `mako` and services like `xdg-desktop-portal` bound to the graphical session.

Unlike `spawn-at-startup`, **this lets you easily monitor their status and output, and *restart* or *reload* them.**

For some softwares provided systemd units out of box, just add them to niri session.

```bash
systemctl --user add-wants niri.service mako.service
systemctl --user add-wants niri.service waybar.service
```

You can check the service of the software you want to add in `/usr/lib/systemd/user/xxx.service`

---

For the software that doesn't provide systemd unit (which means you need to pass a commline argument), you should create the service youself.

For example, `swaybg` doesn't provide systemd unit, you should create `swaybg.service` in `~/.config/systemd/user`
```sh
[Unit]
PartOf=graphical-session.target
After=graphical-session.target
Requisite=graphical-session.target

[Service]
ExecStart=/usr/bin/swaybg -m fill -i "%h/Pictures/LakeSide.png"
Restart=on-failure  
```

Then run
```bash
systemctl --user daemon-reload # update
systemctl --user add-wants niri.service swaybg.service
````

### Delete Service

If you want to remove the service, just delete the symbolic link in `~/.config/systemd/user/niri.service.wants`, then run

```bash
systemctl --user daemon-reload
```
to reload service.

### Running Programs Across Logout

The programs will be killed when logout, if you want to keep the programs, then see [here](https://yalter.github.io/niri/Example-systemd-Setup.html#running-programs-across-logout)

## Important Software

- mako
- xdg-desktop-portal-gtk, xdg-desktop-portal-gnome, gnome-keyring
  - configured in niri-portals.conf
- plasma-polkit-agent
- xawyland-satellite

## workspace

You can name or index your workspace to move.

## Floating Windows

Floating windows in niri always show on top of the tiled windows 

The situation the windows will automatically float:
- they have a parent (e.g. 弹窗验证)
- they are fixed size

You can use keybind to switch between floating and tiling: `toggle-window-floating`.

Or set window rule `open-floating` to `false` to force window.

Use `switch-focus-between-floating-and-tiling` to switch focus between them.

## Tabs

You can switch a column to present windows as tabs, rather than as vertical tiles.

- `toggle-column-tabbed-display`: toggle display

## Overview

The Overview is a zoomed-out view of your workspaces and windows. It lets you see what's going on *at a glance, navigate, and drag windows around*.

- `toggle-overview`:
  - default binding: top-left hot corner; touchpad four-finger swipe up

In the overview, you can
- mouse: left click and drag windows, right click and drag to scroll workspaces, scroll to switch workspaces.
- touchpad: two-finger scrolling that matches the normal three-finger gestures
- touchscreen: one-finger scrolling, or one-finger long press to move a window.

Brief configuration:
```config
// Make workspaces four times smaller than normal in the overview.
overview {
    zoom 0.25
}

// Make the backdrop light.
overview {
    backdrop-color "#777777"
}

// Disable the hot corners.
gestures {
    hot-corners {
        off
    }
}
```

Put a layer-shell wallpaper into the backdrop with a layer rule:
```config
// Make the wallpaper stationary, rather than moving with workspaces.
layer-rule {
    // This is for swaybg; change for other wallpaper tools.
    // Find the right namespace by running niri msg layers.
    match namespace="^wallpaper$"
    place-within-backdrop true
}

// Set transparent workspace background color.
layout {
    background-color "transparent"
}

// Optionally, disable the workspace shadows in the overview.
overview {
    workspace-shadow {
        off
    }
}```

> [!note]
> This will only work for background layer surfaces that ignore exclusive zones (typical for wallpaper tools).


## Screencasting

The primary **screencasting interface** that niri offers is through portals and pipewire. 

use until you setup:
- D-Bus session
- pipewire
- xdg-desktop-portal-gnome
- running niri as a session

main features:

- block out windows: block out particular windows with solid block rectangles.

```config
// Block out password managers from screencasts.
window-rule {
    match app-id=r#"^org\.keepassxc\.KeePassXC$"#
    match app-id=r#"^org\.gnome\.World\.Secrets$"#

    block-out-from "screencast"
}

// Block out mako notifications from screencasts.
layer-rule {
    match namespace="^notifications$"

    block-out-from "screencast"
}
```

- Dynamic screencast target

when select `niri dynamic cast target` as the screencast window, you can use following binds to change what it shows:
  - `set-dynamic-cast-window`: cast the focused window
  - `set-dynamic-cast-monitor`: cast the focused monitor
  - `clear-dynamic-cast-target`: go back to an empty stream

- Indicate screencasted windows

set `window-rule`:
```config
// Indicate screencasted windows with red colors.
window-rule {
    match is-window-cast-target=true

    focus-ring {
        active-color "#f38ba8"
        inactive-color "#7d0d2d"
    }

    border {
        inactive-color "#7d0d2d"
    }

    shadow {
        color "#7d0d2d70"
    }

    tab-indicator {
        active-color "#f38ba8"
        inactive-color "#7d0d2d"
    }
}
```
 
- windowed fullscreen

When screencasting browser-based presentations like Google Slides, you usually want to hide the browser UI, which requires making the browser fullscreen.

The toggle-windowed-fullscreen bind helps with this. It tells the app that it went fullscreen, while in reality leaving it as a normal window that you can resize and put wherever you want.
```config
binds {
    Mod+Ctrl+Shift+F { toggle-windowed-fullscreen; }
}
```

## Layer-Shell Components

Things to keep in mind with layer-shell components (bars, launchers, etc.):

- When a full-screen window is active, other components will be cover.
- Components on the bottom and background layers will receive on-demand keyboard focus as expected. However, they will only receive exclusive keyboard focus when there are no windows on the workspace.
- When opening the Overview, components on the bottom and background layers will zoom out and remain on the workspaces, while the top and overlay layers remain on top of the Overview. So, if you want the bar to remain on top, put it on the top layer.

## Application-Specific Issues / Troubles Shooting

### Electron Apps

Electron-based applications can run directly on Wayland, but it's not the default.

For Electron > 28, you can set an env variable:
```config
environment {
    ELECTRON_OZONE_PLATFORM_HINT "auto"
}
```

For previous versions, pass following flags to app:
```bash
--enable-features=UseOzonePlatform --ozone-platform-hint=auto
```

For desktop entry, put the above flags into the `Exec` section.

### Vscode

tbd

using vscode natively on wayland should pass the right flags:
```bash
code --ozone-platform-hint=auto
```

### WezTerm

tbd

### Zen Brower

screencasting doesn't work out of the box on niri.

To fix it, open `about:config` and set `widget.dmabuf.force-enabled` to true.

### Fullscreen games

tbd

### Steam

tbd

### Waybar and other GTK 3 components

If you have rounded corners on your Waybar and they show up with black pixels in the corners, then set your Waybar opacity to `0.99`, which should fix it.

### Nvidia

There is a quirk in the Nvidia drivers that affects niri's VRAM usage.

Do following steps to fix:
1. `sudo mkdir -p /etc/nvidia/nvidia-application-profiles-rc.d` to make the config dir
2. create file `50-limit-free-buffer-pool-in-wayland-compositors.json` in dir and edit it with
```json
{
    "rules": [
        {
            "pattern": {
                "feature": "procname",
                "matches": "niri"
            },
            "profile": "Limit Free Buffer Pool On Wayland Compositors"
        }
    ],
    "profiles": [
        {
            "name": "Limit Free Buffer Pool On Wayland Compositors",
            "settings": [
                {
                    "key": "GLVidHeapReuseRatio",
                    "value": 0
                }
            ]
        }
    ]
}
```

#### screencast flickering fix

If you have screencast glitches or flickering on NVIDIA, set this in the niri config:
```config
debug {
    wait-for-frame-completion-in-pipewire
}
```

> [!note]
> seems that this bug has been fixed and the debug flag has been removed

### Xwayland

Using `xwayland-satellite` is enough. And niri use it as default.

- [ ] xwayland-satellite should available in `$PATH`

To check that the integration works, run

```bash
$ journalctl --user-unit=niri -b
systemd[2338]: Starting niri.service - A scrollable-tiling Wayland compositor...
niri[2474]: 2025-08-29T04:07:40.043402Z  INFO niri: starting version 25.05.1 (0.0.git.2345.d9833fc1)
(...)
niri[2474]: 2025-08-29T04:07:40.690512Z  INFO niri: listening on Wayland socket: wayland-1
niri[2474]: 2025-08-29T04:07:40.690520Z  INFO niri: IPC listening on: /run/user/1000/niri.wayland-1.2474.sock
niri[2474]: 2025-08-29T04:07:40.700137Z  INFO niri: listening on X11 socket: :0
systemd[2338]: Started niri.service - A scrollable-tiling Wayland compositor.

$ echo $DISPLAY
:0
```

Verifying that the niri output says something like listening on `X11 socket: :0`

## Gestures

- Interactive Move
  - move windows: `Mod` + left mouse
  - Right click while moving to toggle between floating and tiling layout to put the window into
- Interacive Resize: `Mod` + right mouse
- toggle full size
  - Reset window height: double-click the top or bottom tiled window resize edge
  - toggle full width: double-click the left or right tiled window resize edge
- move view using mouse: `mod` + hold `middle button` to move
- Touchpad
  - workspace switch and view movement: three fingers swipes
  - open and close the overview: four fingers vertical swipe
- All Pointing Devices (during drag-and-drop)
  - scroll the view: move the mouse cursor against a monitor edge during DnD
  - hold to activate

## Fullscreen and Maximize

tbd

## Integrating niri

tbd

## Accessibility

tbd

## Miscellaneous

- enable rounded corners for all windows
```kdl
window-rule {
    geometry-corner-radius 12
    clip-to-geometry true
}
```

- How do I recover from a dead screen locker / from a red screen?

You can recover from this by spawning a new screen locker. One way is to switch to a different TTY (with a shortcut like `Ctrl+Alt+F3`) and spawning a screen locker to niri's Wayland display, e.g. `WAYLAND_DISPLAY=wayland-1 swaylock`.


Another way is to set allow-when-locked=true on your screen locker bind, then you can press it on the red screen to get a fresh screen locker.
```kdl
binds {
    Super+Alt+L allow-when-locked=true { spawn "swaylock"; }
}
```

