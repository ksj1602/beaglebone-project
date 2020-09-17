#!/bin/bash

# Script for make clean

execs = ("std" "tcp" "tls")

for exec in "${execs[@]}"
do
    rm -rf "temp-$exec"
done
