#!/bin/bash

msgTag="ScreenCapture"

sendMsg() {
	if [[ -n "$1" ]]; then
		dunstify -a "grim" -u low -h string:x-dunst-stack-tag:$msgTag -i "$HOME/.config/dunst/icons/monitor-screenshot.svg" "Screenshot:" "$1"
	fi
}

msg="Failed to make screenshot"
file="$HOME/Pictures/$(date +%Y%m%d_%Hh%Mm%Ss_grim).png" 

if [[ "$1" == "--full" ]]; then
	grim - | tee "$file" | wl-copy
	msg="Full screenshot saved to ~/Picture"
elif [[ "$1" == "--window" ]]; then
	hyprctl -j activewindow | jq -r '"\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"' | grim -g - - | tee "$file" | wl-copy
	msg="Window screenshot saved to ~/Picture"
elif [[ "$1" == "--area" ]]; then
	if [[ "$2" == "--save" ]]; then
		slurp | grim -g - - | tee "$file" | wl-copy
		msg="Area screenshot saved to ~/Picture"
	elif [[ "$2" == "--show" ]]; then
		slurp | grim -g - - | tee >(wl-copy) | feh -. -Z -
		msg=""
	fi
fi

sendMsg "$msg"
