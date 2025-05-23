import struct
import socket
import sys

def read_pcap(file_path):
    with open(file_path, 'rb') as f:
        # 读取 pcap 全局头（24字节）
        global_header = f.read(24)
        magic_number = struct.unpack('I', global_header[:4])[0]
        
        # 判断字节序（小端或大端）
        if magic_number == 0xa1b2c3d4:
            endian = '<'  # little endian
        elif magic_number == 0xd4c3b2a1:
            endian = '>'  # big endian
        else:
            raise ValueError('不是有效的 pcap 文件')

        count = 0
        # 逐个读取数据包
        while True:
            if count > int(sys.argv[2]):
                break
            packet_header = f.read(16)
            if len(packet_header) < 16:
                break  # 文件结束

            ts_sec, ts_usec, incl_len, orig_len = struct.unpack(endian + 'IIII', packet_header)
            packet_data = f.read(incl_len)

            if len(packet_data) < 20:
                continue  # 非法帧

            version = packet_data[0] >> 4

            if version == 4:
                # IPv4
                if len(packet_data) < 20:
                    continue
                ihl = (packet_data[0] & 0x0F) * 4
                if len(packet_data) < ihl + 4:
                    continue
                protocol = packet_data[9]
                src_ip = socket.inet_ntoa(packet_data[12:16])
                dst_ip = socket.inet_ntoa(packet_data[16:20])

                if protocol in (6, 17):  # TCP or UDP
                    trans_header = packet_data[ihl:ihl+4]
                    if len(trans_header) < 4:
                        continue
                    src_port, dst_port = struct.unpack('!HH', trans_header)
                    print(f"{src_ip}:{src_port} --> {dst_ip}:{dst_port} ({protocol})")
                    count += 1

            elif version == 6:
                # IPv6
                if len(packet_data) < 40:
                    continue
                next_header = packet_data[6]
                src_ip = socket.inet_ntop(socket.AF_INET6, packet_data[8:24])
                dst_ip = socket.inet_ntop(socket.AF_INET6, packet_data[24:40])

                if next_header in (6, 17):  # TCP or UDP
                    trans_header = packet_data[40:44]
                    if len(trans_header) < 4:
                        continue
                    src_port, dst_port = struct.unpack('!HH', trans_header)
                    print(f"{src_ip}:{src_port} --> {dst_ip}:{dst_port} ({protocol})")
                    count += 1

            else:
                # 其他 IP 协议暂不支持
                continue

if __name__ == "__main__":
    for id in range(int(sys.argv[1])):
        print(f"read pcap file: ./data/input/0-{id}.pcap")
        read_pcap(f"./data/input/0-{id}.pcap")
        print()