#!/bin/bash
cd ..
tar xJf dpdk-21.11.6.tar.xz
cd dpdk-21.11.6
meson build
cd build
ninja
meson install
ldconfig
./usertools/dpdk-hugepages.py -p 2M --setup 1G
cd ../../shell