#!/bin/bash

EWW=`which eww`
CFG="$HOME/.config/eww/bar"

## close widgets 
close_eww() {
	${EWW} --config $CFG close-all
}

## Launch or close widgets accordingly
close_eww
