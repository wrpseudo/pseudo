#!/bin/bash

# Verify normal operation...
uid=`env -i id -u`
gid=`env -i id -g`
if [ $uid -ne 0 -o $gid -ne 0 ]; then
    exit 1
fi

export PSEUDO_DISABLED=1
# Verify we dropped OUT of pseudo control, even with env -i
# This checks that env -i replacement functionality still works
# as expected
uid=`env -i id -u`
gid=`env -i id -g`
if [ $uid -eq 0 -o $gid -eq 0 ]; then
    exit 1
fi

export PSEUDO_DISABLED=1
# Verify we can change PSEUDO_DISABLED, even with env -i
# This checks that env -i replacement functionality still works
# as expected
uid=`env -i PSEUDO_DISABLED=0 id -u`
gid=`env -i PSEUDO_DISABLED=0 id -g`
if [ $uid -ne 0 -o $gid -ne 0 ]; then
    exit 1
fi

exit 0
