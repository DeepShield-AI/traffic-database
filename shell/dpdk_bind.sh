#!/bin/bash
cd ../dpdk-21.11.6
./usertools/dpdk-devbind.py --status
./usertools/dpdk-devbind.py --bind=vfio-pci $1