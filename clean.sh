#!/bin/sh

cd src/kernel && make clean
cd ../..

cd src/libc && make clean
cd ../..

cd src/user && make clean
cd ../..

cd src/boot_sector && make clean
cd ../..

cd src/boot_loader && make clean
cd ../..

cd images && make clean
cd ..

