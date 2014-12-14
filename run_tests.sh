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

