#!//usr/bin/env bash
opt_verbose=

usage()
{
    echo >&2 "usage:"
    echo >&2 "  run_tests [-v|--verbose]"
    exit 1
}

for arg
do
        case $arg in
        --)     shift; break ;;
        -v | --verbose)
                opt_verbose=-v
                ;;
        *)
                usage
                ;;
        esac
done

# make sure this is not run as root / group 0 / within pseudo, since
# the fake uid / gid test will fail.
uid=`id -u`
gid=`id -g`
if [ $uid -eq 0 -o $gid -eq 0 ]; then
    echo "ERROR: the tests must not be run as a user with uid=0 (root) or gid=0 (root) or in pseudo itself!"
    exit 1
fi

#The tests will be run on the build dir, not the installed versions
#This requires to following be set properly.
export PSEUDO_PREFIX=${PWD}

num_tests=0
num_passed_tests=0

for file in test/test*.sh
do
    filename=${file#test/}
    let num_tests++
    mkdir -p var/pseudo
    ./bin/pseudo ${opt_verbose} $file
    if [ "$?" -eq "0" ]; then
        let num_passed_tests++
        if [ "${opt_verbose}" == "-v" ]; then
            echo "${filename%.sh}: Passed."
        fi
    else
        echo "${filename/%.sh}: Failed."
    fi
    rm -rf var/pseudo/*
done
echo "${num_passed_tests}/${num_tests} test(s) passed."

