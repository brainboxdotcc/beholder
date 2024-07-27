#!/bin/sh
cd build
# enable core dumps for debugging
# ulimit -c unlimited
# run repeatedly until ctrl+c
while true;
do
	LD_LIBRARY_PATH=/usr/local/lib64 ./beholder
	sleep 60
done

