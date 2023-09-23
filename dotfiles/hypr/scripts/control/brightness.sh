#!/bin/bash

msgTag="myBrightness"

# set brightness according to the input
brightnessctl s $1

# Get current brightness
currentBrightness=$(brightnessctl g)

# Send notification
dunstify -a "Brightness" -u low -i /usr/share/icons/Adwaita/16x16/status/display-brightness-symbolic.symbolic.png -h string:x-dunst-stack-tag:$msgTag -h int:value:"$currentBrightness" "Value: "
