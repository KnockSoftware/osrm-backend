#!/bin/bash

set -e -x

mkdir -p build
cd build
cmake ..
make
ln -sf ../profiles/bicycle.lua profile.lua
