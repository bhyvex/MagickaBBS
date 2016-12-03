#!/bin/bash

cd odoors
make
cd ..
gcc -I./odoors -c main.c
gcc -o magiedit main.o odoors/libs-`uname`/libODoors.a 
