# Waybar

Two bars for niri: a macOS-style light menu bar at the top and a glass dock at
the bottom. `install.sh` links this whole directory to `~/.config/waybar` and
builds the custom CFFI modules.

## Layout

```text
waybar/
├── config                       # Both bars, module order, actions and paths
├── style.css                    # Layout, typography and state styling
├── build.sh                     # Build all three CFFI modules
├── README.md
├── modules/
│   ├── grouped-taskbar.cpp       # Grouped dock with focus-distance sizing
│   ├── pomodoro.cpp             # Persistent click-controlled focus/break timer
│   └── workspace-pips.cpp       # Workspace size and opacity falloff
├── scripts/
│   ├── chinese-calendar.py      # Shanghai clock and Chinese holiday calendar
│   └── control-center.sh        # System stats for tooltip and notification
├── themes/
│   ├── macos-light.css          # Active palette, imported by style.css
│   └── {frappe,latte,macchiato,mocha}.css  # Saved Catppuccin palettes
├── menus/
│   └── apple-menu.xml           # Apple menu structure; actions are in config
├── legacy/
│   └── waybar-default/          # Unmodified stock config and stylesheet
└── build/                      # Generated shared libraries, ignored by Git
    ├── grouped-taskbar.so
    ├── pomodoro.so
    └── workspace-pips.so
```

Edit `config` to change module order or actions, `style.css` for styling, and
`themes/macos-light.css` for colours. The saved Catppuccin palettes use different
colour names; changing the import alone does not replace the active macOS theme.
The stock files in `legacy/` are references and are not loaded by the active bars.

Fonts: Cantarell for text, FontAwesome and Maple Mono NF CN for icons.

## Build and Reload

Arch build dependencies: `base-devel`, `gtk3`, `gtk-layer-shell`, and `jsoncpp`.
`build.sh` works from any working directory and replaces each shared library
only after its compilation succeeds.

After editing C++ sources or moving module files, rebuild and restart Waybar:

```sh
sh ~/dotfiles/dotfiles/waybar/build.sh
systemctl --user restart waybar.service
```

A reload may retain an old shared library. For config-only changes, reload with:

```sh
systemctl --user kill --signal=SIGUSR2 --kill-whom=main waybar.service
```

`module_path` is relative to `$HOME`, the working directory of the user service.
When launching Waybar manually, run it from `$HOME` too. The existing directory
symlink exposes all subfolders; no extra symlinks are needed.

## Grouped Dock

`modules/grouped-taskbar.cpp` shows one icon per app ID across all workspaces and
monitors. Icons come from desktop entries and the current icon theme. A badge
shows the window count when an app has more than one window.

- Left-click a single-window app to focus it immediately.
- Left-click a grouped app to open a window-title menu, then select a window.
- The menu closes after selection, on Escape, or an outside click.
- Hover does not expand the group. Middle/right click does not close windows.

The dock contains running apps only. The focused app keeps its original 28px
icon, its immediate neighbours shrink to 22px, and apps two or more positions
away shrink to 18px. Distance follows the visible dock order. Buttons remain
vertically centred; colours and window-count badges stay the same. With no
focused window, all icons use the smallest size.

The module follows niri's event stream, so opening, closing, renaming, and
focusing windows updates the dock without polling.

## Workspace Indicator

`modules/workspace-pips.cpp` draws one rounded shape per niri workspace. Size
and opacity fall with distance from the focused workspace, symmetrically:

| Distance | Width × height | Opacity with the current theme |
| --- | --- | --- |
| Focused | 24 × 12px | 70% |
| 1 | 4.2 × 10px | 56% |
| 2 | 2.1 × 8px | 42% |
| 3 | 2 × 6px | 28% |
| 4+ | 2 × 4px | 14% |

All indicators use a corner radius of 35% of their shorter side. Geometry and
falloff constants live at the top of the C++ source: `kActiveWidth`,
`kActiveHeight`, `kFaintWidth`, `kFaintHeight`, `kSteps`, `kWidthSteps`,
`kWidthCurve`, `kFaintAlpha`, `kGap`, and `kBandHeight`.

Colours come from `@mac_ws_pip` and `@mac_ws_urgent` in
`themes/macos-light.css`. The module multiplies their alpha by the distance
falloff. `#workspaces` in `style.css` only positions the band. Shapes are
vertically centred in the 30px bar; left-clicking one focuses its workspace.

The module follows `WorkspacesChanged`, `WorkspaceActivated`, and
`WorkspaceUrgencyChanged` events rather than polling. Workspaces from every
output appear in one band; per-output filtering is not implemented.

## Control Center

`scripts/control-center.sh` supplies CPU, memory, swap, root disk usage, and CPU
temperature. Hover the sliders icon to see the stats. Left-click it to show the
same stats in a Mako notification, which disappears after five seconds or when
clicked.

## Pomodoro Timer

`modules/pomodoro.cpp` runs inside Waybar as `cffi/pomodoro`, with no Python
process or per-second state-file writes. A GLib timer updates the countdown
once a second while running; ready and paused states need no timer. Clicks
update immediately, and file notifications synchronize other monitors.
An inactive timer shows only the phase icon; starting shows the countdown,
pausing keeps the remaining time visible, and resetting collapses it again.

- Left-click: start, pause, or resume.
- Right-click: reset to a fresh 25-minute focus session and clear the count.
- Hover: see the phase, completed focus sessions, and controls.

Focus sessions last 25 minutes, short breaks 5 minutes, and every fourth focus
session earns a 15-minute break. Each new phase waits for a click. Mako sends a
notification and plays `mako/sounds/tri-tone.oga` when a phase finishes, via its
`[app-name=Pomodoro]` rule. Focus uses a red clock icon, breaks use a blue coffee
icon, and pause uses a muted pause icon.

State lives in `$XDG_RUNTIME_DIR/waybar-pomodoro/`, shared across monitors and
preserved through Waybar reloads. The C++ module reads the existing Python JSON
format, so switching implementations preserves the session and completed count.
State is written atomically under a shared file lock on clicks and phase changes;
only the instance that commits a phase transition sends its notification.
It clears on logout or reboot. Countdown time uses `CLOCK_BOOTTIME`, including
suspend and ignoring wall-clock adjustments. Durations are defined in `duration()`
in the module.

## Calendar and Apple Menu

`scripts/chinese-calendar.py` displays the clock in `Asia/Shanghai` and supplies
a monthly tooltip with Chinese public holidays and weekends highlighted. Its
holiday periods and makeup workdays are defined for 2026; update the tables
when a new annual schedule is published.

`menus/apple-menu.xml` defines menu entries. Their commands live in
`custom/apple.menu-actions` in `config`, including the app launcher, terminal,
files, lock, suspend, restart, and shutdown actions.
