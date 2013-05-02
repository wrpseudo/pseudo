#!/usr/bin/env bash
mkdir d1
touch d1/f1
mv d1 d2
fileuid=`\ls -n1 d2/f1 | awk '{ print $3 }'`
if [ "$fileuid" == "$UID" ]
then
    #echo "Passed."
    rm -rf d2
    exit 0
fi
rm -rf d2
#echo "Failed"
exit 1
