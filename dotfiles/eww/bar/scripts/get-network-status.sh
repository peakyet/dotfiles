#!/bin/bash

status="$(nmcli networking connectivity)"

if [[ "$status" == "full" ]]; then
	echo "󰖩"
elif [[ "$status" == "unknown" ]]; then
	echo "󱛇"
elif [[ "$status" == "none" ]]; then
	echo "󰖪"
else
        echo "󱚵"
fi
