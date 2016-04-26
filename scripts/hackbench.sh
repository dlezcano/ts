#!/bin/sh

case "$1" in
	run)
		hackbench -p -s 4096 -l 1000
	;;
	*)
	;;
esac
