#!/bin/bash

FILE_PATHS=($PWD/Project)

# Loop through the array and generate the Doxygen for each element.
for i in "${FILE_PATHS[@]}"
do
    # Check if the <path>/doc and <path>/doc/generated directories exist
    if [ -d $i/doc ]; then :
    else mkdir $i/doc
    fi

    if [ -d $i/doc/generated ]; then :
    else mkdir $i/doc/generated
    fi

    pushd $i > /dev/null
    echo "Doxygenating in: $i"
    # Ensure that the doxygen executable is linked as "doxygen" and not "doxygen.exe"
    doxygen > /dev/null
    popd > /dev/null
done
