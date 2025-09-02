import ipaddress

def ipv6_byte_diversity(file_path):
    """
    从文件中读取 IPv6 源/目的地址，分别统计每个字节位上出现了多少种不同字节。
    返回 (src_diversity, dst_diversity) 两个列表，长度均为 16。
    """
    # 分别存储源/目的 16 个字节位置上的集合
    src_sets = [set() for _ in range(16)]
    dst_sets = [set() for _ in range(16)]

    with open(file_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # 格式假设为 ('srcIP', srcPort, 'dstIP', dstPort): count
            try:
                tuple_part, _ = line.split("):")
                parts = eval(tuple_part + ")")  # 恢复成元组
                src_ip = parts[0]
                dst_ip = parts[2]
            except Exception:
                continue  # 跳过异常行

            # 源 IP
            try:
                src_bytes = ipaddress.IPv6Address(src_ip).packed
                for i, b in enumerate(src_bytes):
                    src_sets[i].add(b)
            except Exception:
                pass

            # 目的 IP
            try:
                dst_bytes = ipaddress.IPv6Address(dst_ip).packed
                for i, b in enumerate(dst_bytes):
                    dst_sets[i].add(b)
            except Exception:
                pass

    # 统计每个字节位上不同字节的数量
    src_diversity = [len(s) for s in src_sets]
    dst_diversity = [len(s) for s in dst_sets]
    return src_diversity, dst_diversity


if __name__ == "__main__":
    file_path = "./data/source/flow_ipv6.txt"  # 替换成你的文件
    src_div, dst_div = ipv6_byte_diversity(file_path)

    print("源地址字节多样性：")
    for i, count in enumerate(src_div):
        print(f"  字节位置 {i}: {count} 种不同字节")

    print("\n目的地址字节多样性：")
    for i, count in enumerate(dst_div):
        print(f"  字节位置 {i}: {count} 种不同字节")
