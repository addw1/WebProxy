#!/bin/bash
make clean
make
echo 'Start running proxy server'
./main &
while true ; do continue ; done