#!/bin/bash
echo "Building Fast Trading Terminal..."

mkdir -p build
cd build

# Detect OS and set compiler flags
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"
elif [[ "$OSTYPE" == "msys"* ]]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
else
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

make -j$(nproc)
echo "Build complete!"