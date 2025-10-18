#!/bin/bash

linkFile(){
	# $1: source file
	# $2: symbolic file
	# $3: true for replace the exist file
	# link a single file in dotfiles folder
	# args: file to be linked, linked file (full path), whether to remove the exist file or linksymbol.
	# check if the file or linksymbol exists
	if [ -e $2 -o -L $2 ]
	then
		echo "There already exist file or linksymbol: $2"
		if [ $3 ]
		then
			echo "remove $2"
			rm $2
			# echo "link file $1 to $2"
			ln -sv $1 $2
		fi
	else
		# echo "link file $1 to $2"
		ln -sv $1 $2
	fi
}

# add custom service
customService=(
  "swaybg.service"
  "swayidle.service"
)

echo "start linking custom service to ~/.config/systemd/user"

for srv in "${customService[@]}"
do
  echo "link srv: ${srv}"
  linkFile "$HOME/dotfiles/dotfiles/niri/${srv}" "$HOME/.config/systemd/user/${srv}"
  echo "----------------"
done
# update
systemctl --user daemon-reload # update


# add service
systemctl --user add-wants niri.service waybar.service
systemctl --user add-wants niri.service mako.service
systemctl --user add-wants niri.service mpd.service
systemctl --user add-wants niri.service v2raya-lite.service
systemctl --user add-wants niri.service plasma-polkit-agent.service
systemctl --user add-wants niri.service swaybg.service
systemctl --user add-wants niri.service swayidle.service



