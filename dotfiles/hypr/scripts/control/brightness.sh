#!/bin/bash

msgTag="systemSettings"

# set brightness according to the input
brightnessctl s $1

# Get current brightness
currentBrightness=$(brightnessctl g)

# Send notification
dunstify -a "Brightness" -u low -i "$HOME/.config/dunst/icons/brightness-6.svg" -h string:x-dunst-stack-tag:$msgTag -h int:value:"$currentBrightness" "Value: "
