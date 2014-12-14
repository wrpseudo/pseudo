#!/bin/bash

mode() {
	ls -l "$1" | awk '{ print $1 }'
}

# Verify normal operation...
umask 022
touch a
umask 0
touch b

case $(mode a) in
-rw-r--r--*)	;;
*)	exit 1;;
esac
case $(mode b) in
-rw-rw-rw-*)	;;
*)	exit 1;;
esac

export PSEUDO_DISABLED=1

case $(mode a) in
-rw-r--r--*)	;;
*)	exit 1;;
esac
case $(mode b) in
-rw-r--r--*)	;;
*)	exit 1;;
esac

