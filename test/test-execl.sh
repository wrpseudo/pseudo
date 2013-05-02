#!/usr/bin/env bash
cat > execl_test.c << EOF
#include <unistd.h>
int main() {
        return execl("/usr/bin/env", "/usr/bin/env", "A=A", "B=B", "C=C", NULL);
}
EOF

gcc -o execl_test execl_test.c

./execl_test | grep -q "C=C"

if [ "$?" = "0" ]
then
    #echo "Passed."
    rm -f execl_test execl_test.c
    exit 0
fi
#echo "Failed"
rm -f execl_test execl_test.c
exit 1
