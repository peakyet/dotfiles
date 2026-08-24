#!/bin/bash

allDotfiles=$(ls $HOME/dotfiles/dotfiles)
path="$(pwd)/dotfiles"
excludeConfigs=(
	# "alacritty"
	# "bob"
	# "bashrc"
	# "cava"
	# "dunst"
	# "gitconfig"
	# "helix"
	# "hypr"
	"DSH-Plugins"
	"joshuto"
	# "kitty"
	# "ncmpcpp"
	# "mpd"
	# "p10k.zsh"
	# "tofi"
	"tmux.conf"
	"ueberzugpp"
	# "waybar"
	# "wluma"
	# "xinitrc"
	# "xprofile"
	# "yazi"
	# "zsh"
	# "zshrc"
	# "niri"
	"zshrc_omz"
	"ClaudeCode"
	"readme.md"
	"README.md"
)


linkFile(){
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

linkDotfiles(){
        # link all dotfiles in this repo
        # args: allDotfiles, whether to remove the exist file or linksymbol.
        echo "start linking dotfiles ..."
        for file in $1
        do
					# check if the file is in the exclude list
					if [[ " ${excludeConfigs[@]} " =~ " ${file} " ]]
					then
						echo "exclude ${file}"
					 	echo "-------------------------"
					 	continue
					fi

					sourcefile="$2/$file"
					if [ -d $sourcefile ]
					then
						if [ $file = "zsh" ]
						then
							filesInFolder=$(ls "$sourcefile")
							linkDotfiles "$filesInFolder" $sourcefile $3
						else
							toFile="$HOME/.config/${file}"
							linkFile $sourcefile $toFile $3
						fi
          else
						if [ $file = "zshrc_omz" ]
						then
							toFile="$HOME/.zshrc"
						else
							toFile="$HOME/.${file}"
						fi
						linkFile $sourcefile $toFile $3
          fi
					echo "-------------------------"
	    	done
}

if [ $1 ]
then
  if [ $1 = "force" ]
  then
    linkDotfiles "$allDotfiles" $path true
	else
		echo "Invalid argument: $1"
  fi
else
  linkDotfiles "$allDotfiles" $path
fi

# link the dsh desktop-notify plugin into the dsh web profile so it can be
# loaded via ~/.dsh/profiles/web/cordis.patch.yml (name: './dsh-desktop-notify.js')
dshProfileDir="$HOME/.dsh/profiles/web"
echo "start linking dsh desktop-notify plugin to $dshProfileDir"
if [ ! -d "$dshProfileDir" ]
then
	echo "create $dshProfileDir"
	mkdir -p "$dshProfileDir"
fi
if [ "$1" = "force" ]
then
	linkFile "$path/DSH-Plugins/dsh-desktop-notify/notify.js" "$dshProfileDir/dsh-desktop-notify.js" true
else
	linkFile "$path/DSH-Plugins/dsh-desktop-notify/notify.js" "$dshProfileDir/dsh-desktop-notify.js"
fi
echo "-------------------------"

# set service of niri
# source "${path}/niri/add_wants.sh"
sh "${path}/niri/add_wants.sh"
