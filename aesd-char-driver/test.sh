#!/bin/sh

echo "hello" > /dev/aesdchar
echo "cruel" > /dev/aesdchar
echo "world" > /dev/aesdchar

# head -c 19 /dev/aesdchar

cat /dev/aesdchar

# var=$(dmesg)
# echo "$var"