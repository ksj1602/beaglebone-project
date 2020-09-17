#!/bin/bash

# Script for compiling the code

FLAGS="-Wall -Wextra -lm"

if [[ $1 = "std" ]]
then
    EXEC="temp-std"
    PROG="std.c"
elif [[ $1 = "tcp" ]]
then
    EXEC="temp-tcp"
    PROG="tcp.c"
elif [[ $1 = "tls" ]]
then
    EXEC="temp-tls"
    PROG="tls.c"
    FLAGS+=" -lssl -lcrypto"
else
    echo "Must specify std, tcp or tls compilation option"
    echo "Usage: ./compile.sh [std | tcp | tls]"
    exit 1
fi

DUMMY=$(uname -a | grep "armv7l")

if [[ "$DUMMY" ]] # on an embedded system platform like BeagleBone Green Wireless
then
    FLAGS+=" -lmraa"
    gcc $PROG -o $EXEC $FLAGS

# not on beaglebone
else 
    gcc $PROG -o $EXEC $FLAGS
fi
