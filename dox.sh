#/bin/bash

CURR=$PWD
FILE_PATHS=($CURR/Project)

for i in "${FILE_PATHS[@]}"
do
    cd $i
    echo "Doxygenating in $i"
    doxygen.exe > nul 2>&1
done

cd $CURR