#! /bin/bash

if [ $# -ne 2 ]
    then
        echo "Needs 2 arguments supplied"
        exit 1        
fi


if [ ! -d $1 ]
    then
        echo "Specified directory doesn't exist"
        exit 1
fi

X=$(find $1 -type f | wc -l)
Y=$(grep -Rwo "$1" -e "$2" | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
