#!/bin/bash

# Test if we re-invoke pseudo that chroot still works

# Return vals: 2 - invalid arg list
#              1 - chroot failed
#              0 - chroot succeeded
cat > chroot_test.c << EOF
#include <unistd.h>
int main(int argc, char *argv[]) {
        if (argc != 2)
          return 2;
        return (chroot(argv[1]) == -1);
}
EOF

gcc -o chroot_test chroot_test.c

# The following should just run chroot_test since pseudo is already loaded
./bin/pseudo ./chroot_test `pwd`

if [ "$?" = "0" ]
then
    #echo "Passed."
    rm -f chroot_test chroot_test.c
    exit 0
fi
#echo "Failed"
rm -f chroot_test chroot_test.c
exit 1
