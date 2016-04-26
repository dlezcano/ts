#!/bin/sh

case "$1" in
	prerun)
	;;
	run)
		mplayer ./data/clipcanvas_14348_offline.mp4
	;;
	postrun)
	;;
	*)
		echo "Unexpected value passed as parameter '$1'" 1>&2
	;;
esac
