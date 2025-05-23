# Traffic database (CraftIndex)

## 1.Background
### goals
* Real time capture of high-speed traffic
* High parallelism index construction
* Index storage compression
* Index key customization

### Technology Dependencies
* Programming Language: C++, version 17
* DPDK, version 21.11.6

## 2.Build & run
### Dependencies library install
* We provide the script for dependency installation `shell/depencdency_install.sh` 
	* `sudo` is needed
* You can also manually install dependencies as follows
	* libpcap-dev: `apt install libpcap-dev`
	* build-essential: `apt install build-essential`
	* meson: `apt install meson`
	* python3-pyelftools: `apt install python3-pyelftools`
	* pkg-config: `apt install pkg-config`

### DPDK install (Reference: [https://doc.dpdk.org/guides/linux_gsg/index.html](https://doc.dpdk.org/guides/linux_gsg/index.html))
* We provide DPDK installation package and the script `shell/depencdency_install.sh`
	* This script needs to be run in the `shell` folder
	* `sudo` is needed
* You can also download and install DPDK as follows
	* Download DPDK (version 21.11.6) from [https://core.dpdk.org/download/](https://core.dpdk.org/download/)
	* Compile & build DPDK as following code:

```
tar xJf dpdk-<version>.tar.xz
cd dpdk-<version>
meson build
cd build
ninja
(sudo) meson install
(sudo) ldconfig
```

### NIC bind

* The network card used to capture data packets needs to be bound to DPDK according to the following steps(Reference: [https://doc.dpdk.org/guides/linux_gsg/linux_drivers.html](https://doc.dpdk.org/guides/linux_gsg/linux_drivers.html))
* We provide the binding script `shell/dpdk_bind.sh`
	* It should be running with NIC name as the input param, such as `./dpdk_bind.sh ens5f1`
	* This script needs to be run in the `shell` folder
	* `sudo` is needed
	* !!! Warning: once the NIC is bound to DPDK, all its packets will not be received by the kernel protocol stack and applications. So do not bind any NICs currently being used by applications other than this project to the DPDK. Specifically, if you are using remote connection tools such as SSH, binding the NIC used for connection to DPDK will cause the connection to disconnect!
* You can aslo bind NIC manually as the following code:

```
cd dpdk-<version>
# Default <version> is 21.11.6

# To see the status of all network ports on the system
./usertools/dpdk-devbind.py --status
# The output should be like this
# Network devices using kernel driver
# ===================================
# 0000:86:00.1 'Ethernet Controller XL710 for 40GbE QSFP+ 1583' if=ens5f1 drv=i40e unused=vfio-pci *Active*
# ...


# <NIC> is the NIC that needs to receive traffic, e.g.
./usertools/dpdk-devbind.py --bind=vfio-pci <NIC>
# LIKE: ./usertools/dpdk-devbind.py --bind=vfio-pci ens5f1
# OR: ./usertools/dpdk-devbind.py --bind=vfio-pci 0000:86:00.1
# If you need to unbind NIC from DPDK: ./usertools/dpdk-devbind.py -u 0000:86:00.1
```

### System build & run
* Create necessary folders (You can also run the script `shell/make_dir.sh` under `shell` folder)

```
mkdir build
mkdir data
mkdir data/source
mkdir data/index
mkdir data/output
```

* Make project

```
make
```

* Run

```
(sudo) ./build/dpdkControllerTest
```

### Parameter Description
* Users can input parameters to set:
	* whether binding to cores (If there are no performance requirements, there is no need to bind cores)
	* the number of DPDK packet capture threads (also the number of packet processing threads)
	* the number of indexing threads in the thread pool
	* the cores to bind (if needed)
* Example 1 (without binding cores, 2 DPDK packet capture threads and 4 indexing threads)

```
Do you want to bind to cores? (y/n)
n
Enter number of DPDK packet capture threads
2
Enter number of indexing thread
4
...
[Press any key to quit]
```
* Example 2 (with binding cores, note that the core 0 is remained for DPDK TX thread)

```
Do you want to bind to cores? (y/n)
y
Enter the controller core number (0 is remained)
20
Enter number of DPDK packet capture threads
2
Enter the core number for each DPDK packet capture threads (0 is remained)
2 4
Enter the core number for each packet processing threads (0 is remained)
6 8
Enter number of indexing thread
4
Enter the core number for each indexing threads (0 is remained)
10 12 14 16
...
[Press any key to quit]
```

## 4.Results Display
* Use the packet sending tool to send packets to the DPDK bound NIC.
* The data files are saved at `data/input`. 
	* the naming convention is `[NIC]_[Capture thread number].pcap`, e.g. `0-0.pcap`
	* compared to regular PCAP files, the format may be different and may not be readable using Wireshark
* The index files are saved at `data/index`.
* The data files can be checked by `readpcap.py`
	* running `readpcap.py` as `python3 readpcap.py [DPDK_Thread_Count] [Displayed_Packet_Count_For_Each_File]`
	* such as `python3 readpcap.py 2 10`
	* the output should be like:

```
read pcap file: ./data/input/0-0.pcap
133.218.212.225:389 --> 187.160.222.201:10161 (17)
213.200.226.103:57689 --> 203.194.187.104:22 (6)
133.199.122.42:3490 --> 5.245.200.8:34592 (6)
133.218.212.225:0 --> 187.160.211.10:0 (17)
133.218.212.225:0 --> 187.160.211.10:0 (17)
133.218.212.225:0 --> 187.207.0.105:0 (17)
133.218.212.225:0 --> 187.207.0.105:0 (17)
133.218.212.225:0 --> 177.125.253.22:0 (17)
133.218.212.225:0 --> 177.125.253.22:0 (17)
133.199.122.42:3490 --> 5.245.200.8:34592 (6)
54.134.212.246:56564 --> 203.194.180.138:30303 (6)

read pcap file: ./data/input/0-1.pcap
133.199.122.26:21 --> 202.229.236.107:7614 (6)
185.6.43.253:8310 --> 133.199.122.47:8121 (6)
185.6.43.253:8310 --> 133.199.122.47:8121 (6)
163.189.42.216:50902 --> 52.83.47.45:443 (6)
163.189.42.216:50902 --> 52.83.47.45:443 (6)
218.225.224.244:51202 --> 203.194.184.81:873 (6)
163.189.42.216:50902 --> 52.83.47.45:443 (6)
185.6.43.253:8310 --> 133.199.122.47:8121 (6)
163.189.42.216:50902 --> 52.83.47.45:443 (6)
185.6.43.253:8310 --> 133.199.122.47:8121 (6)
185.6.43.253:8310 --> 133.199.122.47:8121 (6)
```

## 5.The structure of codes
* **dpdk_lib**: the manually written dependency libraries required for the project, including indexed data structures, lock free read-write ring structures, etc
* **dpdk_component**: the main code of project components
* **bpf**: package tagging code in BPF format
* **test**: test code, includes:
	* dpdkControllerTest.cpp: main file of the program
	* indexCompressTest.cpp: the code to test the performance of index compression
	* indexTest.cpp: the code to test the query latency of indexes
	* ringTest.cpp: the code to test read/write speed of lock-free read-write ring
	* (All code in this folder needs to be compiled using the corresponding makefile; Except for dpdkControlTest.cpp, all other files do not require dpdk dependencies for compilation and operation)
* **experiment**: the experiment data is stored in this folder
