#!/bin/sh

case "$1" in
	prerun)
	;;
	run)
		mplayer ./data/John_Wesley_Coleman_-_07_-_Tequila_10_Seconds.mp3
	;;
	postrun)
	;;
	*)
		echo "Unexpected value passed as parameter '$1'" 1>&2
	;;
esac
