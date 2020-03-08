#/bin/bash

CURR=$PWD
FILE_PATHS=($CURR/Project)


for i in "${FILE_PATHS[@]}"
do
    # Check if the doc and doc/generated directories exist
    if [ -d $i/doc ]; then :
    else mkdir $i/doc
    fi

    if [ -d $i/doc/generated ]; then :
    else mkdir $i/doc/generated
    fi

    cd $i
    echo "Doxygenating in $i"
    doxygen.exe > /dev/null 2>&1
done

cd $CURR