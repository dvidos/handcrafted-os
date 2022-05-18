#!/bin/sh

cd src/kernel && make clean
cd ../..
cd src/libc && make clean
cd ../..
cd src/user && make clean
cd ../..


