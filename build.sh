#!/bin/bash
BUILD_TYPE=${1:-Debug}

mkdir -p build/$BUILD_TYPE
cd build/$BUILD_TYPE
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../..
cmake --build . -j$(nproc)
