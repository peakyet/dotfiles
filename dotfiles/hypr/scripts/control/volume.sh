#!/bin/bash

msgTag="myVolume"

amixer set Master $1

# Query amixer for the current volume and whether or not the speaker is muted
volume="$(amixer -c 0 get Master | tail -1 | awk '{print $4}' | sed 's/[^0-9]*//g')"
mute="$(amixer -c 0 get Master | tail -1 | awk '{print $6}' | sed 's/[^a-z]*//g')"


if [[ $volume == 0 || "$mute" == "off" ]]; then
    # Show the sound muted notification
    dunstify -a "Volume" -u low -i /usr/share/icons/Adwaita/16x16/status/audio-volume-muted-symbolic.symbolic.png -h string:x-dunst-stack-tag:$msgTag -h int:value:"$volume" "Status: $mute, Value: " 
else
    # Show the volume notification
    dunstify -a "Volume" -u low -i /usr/share/icons/Adwaita/16x16/status/audio-volume-high-symbolic.symbolic.png -h string:x-dunst-stack-tag:$msgTag -h int:value:"$volume" "Status: $mute, Value: "
fi

# Play the volume changed sound
# canberra-gtk-play -i audio-volume-change -d "changeVolume"
