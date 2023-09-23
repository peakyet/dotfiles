#!/bin/bash

msgTag="myVolume"

# Query the volume before changing it
volumeBefore="$(amixer -c 0 get Master | tail -1 | awk '{print $4}' | sed 's/[^0-9]*//g')"

amixer set Master $1

# Query amixer for the current volume and whether or not the speaker is muted
volume="$(amixer -c 0 get Master | tail -1 | awk '{print $4}' | sed 's/[^0-9]*//g')"
mute="$(amixer -c 0 get Master | tail -1 | awk '{print $6}' | sed 's/[^a-z]*//g')"


if [[ $volume == 0 || "$mute" == "off" ]]; then
    # Show the sound muted notification
    dunstify -a "Volume" -u low -i $HOME/.config/dunst/icons/volume/volume-off.svg -h string:x-dunst-stack-tag:$msgTag -h int:value:"$volume" "Status: $mute, Value: " 
else
    # Show the volume notification
    if [ $volumeBefore -lt $volume ]
    then
	    dunstify -a "Volume" -u low -i $HOME/.config/dunst/icons/volume/volume-plus.svg -h string:x-dunst-stack-tag:$msgTag -h int:value:"$volume" "Status: $mute, Value: "
    else
	    dunstify -a "Volume" -u low -i $HOME/.config/dunst/icons/volume/volume-minus.svg -h string:x-dunst-stack-tag:$msgTag -h int:value:"$volume" "Status: $mute, Value: "
    fi
fi

# Play the volume changed sound
# canberra-gtk-play -i audio-volume-change -d "changeVolume"
