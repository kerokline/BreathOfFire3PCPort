"""Read a Mednafen -soundrecord WAV even when the process was killed (header sizes unset)."""
import struct, sys
import numpy as np


def read(path):
    b = open(path, "rb").read()
    assert b[:4] == b"RIFF" and b[8:12] == b"WAVE", b[:12]
    p, fmt, data = 12, None, None
    while p + 8 <= len(b):
        cid, sz = b[p:p + 4], struct.unpack_from("<I", b, p + 4)[0]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", b, p + 8)
        if cid == b"data":
            data = b[p + 8:] if sz == 0 or p + 8 + sz > len(b) else b[p + 8:p + 8 + sz]
            break
        p += 8 + sz + (sz & 1)
    tag, ch, sr, _, align, bits = fmt
    assert tag == 1 and bits == 16, fmt
    n = len(data) // align
    x = np.frombuffer(data[:n * align], dtype="<i2").reshape(-1, ch).astype(np.float32) / 32768.0
    return x, sr


if __name__ == "__main__":
    x, sr = read(sys.argv[1])
    a = np.abs(x).max(axis=1)
    print("sr", sr, "ch", x.shape[1], "seconds", len(x) / sr, "peak", a.max())
    for s in range(0, len(x) // sr, 2):
        seg = x[s * sr:(s + 2) * sr]
        print(s, round(20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-9), 1), end=" | ")
    print()
