#!/usr/bin/env python3
"""Group a Logic 2 I2C data table into register reads; report WAKE_UP_SRC (0x1B) values and
OUTX..OUTZ_A (0x28..0x2D, +/-4 g -> 0.122 mg/LSB) samples around given times."""
import csv, sys
path = sys.argv[1]; windows = [float(x) for x in sys.argv[2:]]
tx, cur = [], None  # each: [t, reg, [read bytes]]
with open(path) as f:
    for r in csv.DictReader(f):
        t = float(r["start_time"])
        if r["type"] == "address" and r["read"] == "false":
            cur = [t, None, []]; tx.append(cur)
        elif r["type"] == "data" and cur is not None:
            if cur[1] is None: cur[1] = int(r["data"], 16)
            else: cur[2].append(int(r["data"], 16))
        elif r["type"] == "address" and r["read"] == "true":
            pass
wu = [(t, b[0]) for t, reg, b in tx if reg == 0x1B and b]
print(f"transactions {len(tx)}, WAKE_UP_SRC reads {len(wu)}")
for t, v in wu:
    if v & 0x08: print(f"  WU_IA set t={t:.4f} WAKE_UP_SRC=0x{v:02x}")
def s16(lo, hi): v = lo | hi << 8; return v - 65536 if v > 32767 else v
acc = [(t, [s16(b[i], b[i+1]) * 0.122 for i in (0, 2, 4)]) for t, reg, b in tx if reg == 0x28 and len(b) >= 6]
print(f"accel reads {len(acc)}")
for w in windows:
    pts = [(t, a) for t, a in acc if w - 1.0 <= t <= w + 1.5]
    jumps = [(pts[i][0], [round(pts[i][1][k] - pts[i-1][1][k]) for k in range(3)]) for i in range(1, len(pts))]
    big = max(jumps, key=lambda j: max(abs(x) for x in j[1])) if jumps else None
    print(f"window {w-1:.1f}-{w+1.5:.1f}s: largest 40ms-poll delta {big}")
    regs = sorted({reg for t, reg, b in tx if w - 1 <= t <= w + 1.5})
    print(f"   regs read: {[hex(r) for r in regs if r is not None]}")
