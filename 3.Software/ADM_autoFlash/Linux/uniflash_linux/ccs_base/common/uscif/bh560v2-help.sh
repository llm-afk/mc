#!/bin/bash

if [ -f /usr/bin/gnome-open ];then
	gnome-open $1

elif [ -f /usr/bin/xdg-open ];then
	xdg-open $1

elif [ -f /usr/bin/gvfs-open ];then
	gvfs-open $1
fi

exit
