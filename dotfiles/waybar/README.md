# Waybar Styling Guide

This document explains how to configure the `style.css` file for Waybar, a highly customizable Wayland bar for Sway and other Wayland compositors.

## Control Center

Left-click the sliders icon next to the calendar to show current CPU, memory,
swap, root disk usage, and temperature in a Mako notification. It disappears
after five seconds, or sooner if clicked, and works even when Waybar's hover tooltip fails.
Hover still shows the same stats. Both use `control-center.sh`.

## Pomodoro Timer

The timer in the upper bar's status area, between the battery and Control Center,
is powered by `pomodoro.py` (Python standard library only). It updates once a
second without adding another daemon and matches the menu bar's compact styling.
While inactive or waiting for the next session, it shows only the phase icon.
Starting expands it to show the countdown; pausing keeps the remaining time
visible, and resetting collapses it back to the icon.

- **Left click:** start, pause, or resume the countdown.
- **Right click:** reset to a fresh 25-minute focus session and clear the session count.
- **Hover:** see the current phase, completed focus sessions, and controls.

Focus sessions last 25 minutes, short breaks 5 minutes, and every fourth completed
focus session earns a 15-minute break. Mako displays a message and plays the
tri-tone chime from `mako/sounds/tri-tone.oga` whenever a focus session, short
break, or long break finishes, using the `[app-name=Pomodoro]` rule in its config.
Click the timer to start the next phase. The clock icon turns red
during focus, the coffee icon turns blue during a break, and pause shows a muted
pause icon. Only completed focus sessions count toward the long break.

State is stored in `$XDG_RUNTIME_DIR/waybar-pomodoro/`, shared across monitors and
preserved when Waybar reloads. It clears with the runtime directory on logout or
reboot. Countdown time includes suspend and ignores system clock adjustments.
Durations are defined in `DURATIONS` in `pomodoro.py`.

After editing the config, reload the running bar with:

```sh
systemctl --user kill --signal=SIGUSR2 --kill-whom=main waybar.service
```

## Overview

The `style.css` file uses a standard CSS syntax to control the appearance of the Waybar and its modules. This allows for a great deal of flexibility in matching the bar to your desktop's theme.

This particular configuration uses the Catppuccin color theme, with the color palette defined in separate files (`mocha.css`, `latte.css`, etc.) and imported into the main `style.css`.

## File Structure

*   **`style.css`**: The main stylesheet that Waybar loads. It contains the core styling rules.
*   **`mocha.css` / `latte.css` / etc.**: These files define the color palettes for different themes. The `@import` rule at the top of `style.css` determines which theme is active.

## Basic Configuration

### Changing the Theme

To change the color theme, you can modify the `@import` rule at the top of `style.css`. For example, to switch from the "Mocha" theme to the "Latte" theme, you would change:

```css
@import "mocha.css";
```

to:

```css
@import "latte.css";
```

### General Bar Styling

The entire Waybar is styled using the `window#waybar` selector. Here, you can change properties like the background color, borders, and text color for the entire bar.

```css
window#waybar {
    background-color: transparent;
    border-bottom: 3px solid rgba(100, 114, 125, 0.5);
    color: @text;
}
```

In this configuration, the bar has a transparent background and a semi-transparent bottom border. The text color is set to the `@text` variable defined in the imported theme file.

### Styling Individual Modules

Each module in Waybar has a unique ID that you can use as a CSS selector to apply specific styles. For example, to style the clock module, you would use the `#clock` selector.

```css
#clock {
    background-color: transparent;
    box-shadow: inset 0 -3px @rosewater;
    color: @rosewater;
}
```

In this configuration, most modules have a transparent background and a colored bottom border created with an inset `box-shadow`. This gives the appearance of a colored underline for each module.

### Styling Module States

Many modules have different states that can be styled individually. These states are represented by CSS classes. For example, the battery module has `.charging`, `.plugged`, and `.critical` states.

```css
#battery.charging, #battery.plugged {
    background-color: transparent;
    box-shadow: inset 0 -3px @pink;
    color: @pink;
}

#battery.critical:not(.charging) {
    color: @pink;
    box-shadow: inset 0 -3px @pink;
    animation-name: blink;
    animation-duration: 0.5s;
    animation-timing-function: linear;
    animation-iteration-count: infinite;
    animation-direction: alternate;
}
```

This allows you to have different styles for when the battery is charging, full, or critically low.

### The #window Module and Animated Backgrounds

A particularly interesting part of this configuration is the styling for the `#window` module, which displays the title of the currently focused window.

```css
#window {
    border-radius: 20px;
    padding-left: 10px;
    padding-right: 10px;
    color: white;
    background: radial-gradient(circle, rgba(137,180,250,120) 0%, ...); 
    background-size: 400% 400%;
    /* animation: gradient_f 20s cubic-bezier(.72,.39,.21,1) infinite; */
}

@keyframes gradient_f {
	0% {
		background-position: 0% 200%;
	}
	50% {
		background-position: 200% 0%;
	}
	100% {
		background-position: 400% 200%;
	}
}
```

This module has a complex `radial-gradient` as its background, which creates a colorful and dynamic look.

The configuration also includes a commented-out CSS animation (`gradient_f`). If you uncomment the `animation` property in the `#window` selector, this will create a smoothly moving gradient effect.

**Caution:** Enabling this animation may increase CPU usage, as rendering complex animations can be resource-intensive. If you notice performance issues after enabling it, you may want to comment it out again.

### Fonts and Icons

You can set the font and font size for the entire bar in the `*` (universal) selector.

```css
* {
    font-family: FontAwesome, Maple Mono NF CN;
    font-size: 17px;
}
```

This configuration uses "FontAwesome" for icons and "Maple Mono NF CN" (a Nerd Font) for text. Make sure you have these fonts installed on your system.

## Customization Example

Let's say you want to change the color of the CPU module to green.

1.  **Find the color:** Look in the imported theme file (e.g., `mocha.css`) for a green color. You'll find `@define-color green #a6e3a1;`.
2.  **Find the module's CSS:** In `style.css`, find the `#cpu` selector.
3.  **Change the color:** Modify the `color` and `box-shadow` properties to use the `@green` variable.

```css
#cpu {
    background-color: transparent;
    box-shadow: inset 0 -3px @green;
    color: @green;
}
```

By following these principles, you can customize the look and feel of your Waybar to your liking. For a full list of modules and their styling options, refer to the [Waybar documentation](https://github.com/Alexays/Waybar/wiki/Styling).
