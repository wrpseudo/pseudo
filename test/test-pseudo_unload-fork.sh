#!/bin/bash

# Verify normal operation...
uid=`id -u`
gid=`id -g`
if [ $uid -ne 0 -o $gid -ne 0 ]; then
    exit 1
fi

export PSEUDO_UNLOAD=1
# Verify we dropped OUT of pseudo control
uid=`id -u`
gid=`id -g`
if [ $uid -eq 0 -o $gid -eq 0 ]; then
    exit 1
fi

exit 0
