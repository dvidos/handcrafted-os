#!/bin/sh

# stop on first failure
set -e

echo Making libc...
cd src/libc
make test  # makes and runs tests
make
cd ../..

echo Making kernel...
cd src/kernel
make test  # makes and runs tests
make
cd ../..

echo Making user programs...
cd src/user
make
cd ../..

echo Making boot sector...
cd src/boot_sector
make
cd ../..

echo Making boot loader...
cd src/boot_loader
make
cd ../..

echo Making images...
cd images
make
cd ../..

echo Build finished without errors! Run or burn images as needed.
