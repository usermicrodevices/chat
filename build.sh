#!/bin/bash
#rm -rf build/
mkdir -p build
cd build
#cmake .. > ../build.log 2>&1
#make -j$(nproc) >> ../build.log 2>&1
#cmake .. --trace-expand
cmake ..
make -j$(nproc)
cd ..
cp config.json build/
