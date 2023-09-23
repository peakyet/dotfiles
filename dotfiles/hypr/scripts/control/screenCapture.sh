#!/bin/bash

msgTag="ScreenCapture"

grim

dunstify -a "grim" -u low -h string:x-dunst-stack-tag:$msgTag -i "$HOME/.config/dunst/icons/monitor-screenshot.svg" "Screenshot:" "save to Picture"
