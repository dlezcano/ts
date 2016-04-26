#!/bin/sh

case "$1" in
	prerun)
		echo $1
	;;
	run)
		echo $1
	;;
	postrun)
		echo $1
	;;
	*)
		echo "Unexpected value passed as parameter --$1--"
	;;
esac
