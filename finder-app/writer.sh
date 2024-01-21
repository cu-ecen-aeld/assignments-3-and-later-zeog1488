#! /bin/bash

if [ $# -ne 2 ]
    then
        echo "Needs 2 arguments supplied"
        exit 1        
fi

$(mkdir -p "$(dirname "$1")" && touch "$1")

if [ $? -eq 1 ]
    then
        echo "File creation error"
        exit 1
fi

echo $2 > $1