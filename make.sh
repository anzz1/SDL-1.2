#!/bin/sh
if [ ! -f Makefile ];
then
	./config.sh
fi
make -j
./strip.sh
