#!/bin/bash

# Return vals: 2 - Unable to run xattr commands
#              1 - Invalid return value
#              0 - Pass

touch f1
attrs=`getfattr -d f1 | grep -v '^#'`
if [ -n "$attrs" ]
then
    #echo "Fail, unexpected getfattr result '$attr'"
    rm -f f1
    exit 1
fi

setfattr -n "user.dummy" -v "test_f1" f1
if [ $? -ne 0 ]
then
    #echo "Fail, unable to call setfattr"
    rm -f f1
    exit 2
fi

attrs=`getfattr -d f1 | grep -v '^#'`
if [ "$attrs" != 'user.dummy="test_f1"' ]
then
    #echo "Fail, unexpected getfattr result '$attr'"
    rm -f f1
    exit 1
fi

setfattr -n "security.dummy" -v "test_f2" f1 
if [ $? -ne 0 ]
then
    #echo "Fail, unable to call setfattr"
    rm -f f1
    exit 2
fi

attrs=`getfattr -n "security.dummy" f1 | grep -v '^#'`
if [ "$attrs" != 'security.dummy="test_f2"' ]
then
    #echo "Fail, unexpected getfattr result '$attr'"
    rm -f f1
    exit 1
fi

#echo "Passed."
rm -f f1
exit 0
