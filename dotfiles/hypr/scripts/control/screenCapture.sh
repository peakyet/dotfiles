#!/bin/bash

msgTag="ScreenCapture"

grim

dunstify -a "grim" -u low -h string:x-dunst-stack-tag:$msgTag "Screenshot:" "save to Picture"
