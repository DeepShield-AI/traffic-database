import ipaddress
import numpy as np
from collections import defaultdict

def ipv6_to_bytes(addr: str) -> np.ndarray:
    """IPv6 -> 16字节 numpy.uint8 数组"""
    return np.frombuffer(ipaddress.IPv6Address(addr).packed, dtype=np.uint8)

def parse_file(filename: str):
    """解析文件，提取唯一源/目的 IPv6 地址"""
    src_addrs, dst_addrs = set(), set()
    with open(filename, "r") as f:
        for line in f:
            parts = line.strip().split("'")
            if len(parts) < 4:
                continue
            src_addrs.add(parts[1])
            dst_addrs.add(parts[3])
    return list(src_addrs), list(dst_addrs)

def compute_min_diffs_numpy(addrs, batch_size=2000):
    """
    使用 NumPy 分块计算 IPv6 地址的最小字节差异
    忽略“仅最后一个字节不同”的情况
    """
    N = len(addrs)
    addr_bytes = np.array([ipv6_to_bytes(a) for a in addrs], dtype=np.uint8)  # (N,16)
    min_diffs = np.full(N, 16, dtype=np.int32)

    for i in range(0, N, batch_size):
        batch = addr_bytes[i:i+batch_size]  # (B,16)

        # 广播比较 -> (B,N,16) 布尔矩阵
        neq = np.not_equal(batch[:, None, :], addr_bytes[None, :, :])
        diffs = neq.sum(axis=2)  # (B,N) 差异字节数

        # --- 新增排除规则 ---
        # 条件：前14字节都相同，且最后两个字节至少有一个不同
        only_last2_diff = (np.all(~neq[:, :, :-2], axis=2)) & (np.any(neq[:, :, -2:], axis=2))

        # 把这种情况设置为最大值（16），不参与最小差异比较
        diffs[only_last2_diff] = 16

        # 自己和自己差异设为最大值
        np.fill_diagonal(diffs[:, i:i+batch_size], 16)

        # 取最小差异
        min_diffs[i:i+batch_size] = np.min(diffs, axis=1)

    return {addr: diff for addr, diff in zip(addrs, min_diffs)}

def summarize(min_diffs):
    """统计 {差异字节数: 地址数量}"""
    summary = defaultdict(int)
    for diff in min_diffs.values():
        summary[diff] += 1
    return dict(sorted(summary.items()))

if __name__ == "__main__":
    filename = "./data/source/flow_ipv6.txt"

    # 解析唯一地址
    src_addrs, dst_addrs = parse_file(filename)

    print(f"源地址数: {len(src_addrs)}, 目的地址数: {len(dst_addrs)}")

    # 分别计算
    src_min_diffs = compute_min_diffs_numpy(src_addrs, batch_size=2000)
    print("源地址最小差异统计:", summarize(src_min_diffs))
    dst_min_diffs = compute_min_diffs_numpy(dst_addrs, batch_size=2000)
    print("目的地址最小差异统计:", summarize(dst_min_diffs))
