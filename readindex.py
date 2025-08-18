import struct
import socket
import sys

def read_pcap(file_path):
    with open(file_path, 'rb') as f:
        # 读取 pcap 全局头（24字节）
        # global_header = f.read(24)
        # magic_number = struct.unpack('I', global_header[:4])[0]
        
        # # 判断字节序（小端或大端）
        # if magic_number == 0xa1b2c3d4:
        #     endian = '<'  # little endian
        # elif magic_number == 0xd4c3b2a1:
        #     endian = '>'  # big endian
        # else:
        #     raise ValueError('不是有效的 pcap 文件')

        endian = '<'
        count = 0
        
        f.seek(1099511627776)
        
        # 逐个读取数据包
        while True:
            count += 1
            if count > int(sys.argv[2]):
                break
            
            ip = f.read(4)
            print(ip[0],ip[1],ip[2],ip[3])
            f.read(8)
            
            

if __name__ == "__main__":
    # for id in range(int(sys.argv[1])):
    #     print(f"read pcap file: ./data/input/0-{id}.pcap")
    #     read_pcap(f"./data/input/0-{id}.pcap")
    #     print()
    filename = sys.argv[1]
    print(f"read pcap file: {filename}")
    read_pcap(filename)