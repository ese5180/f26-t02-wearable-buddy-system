#!/usr/bin/env python3
"""Impact pass test T1-T7 from a Saleae raw CSV (ch0 marker, ch1 SDA, ch2 SCL) + host-stamped serial logs.
usage: analyse.py digital.csv tag.log counselor.log saleae_start_epoch
"""
import csv, re, statistics, sys

dig, taglog, cnslog, t0 = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
rows = []
with open(dig) as f:
    r = csv.reader(f); next(r)
    for x in r:
        if x: rows.append((float(x[0]), int(x[1]), int(x[2]), int(x[3])))
span = rows[-1][0]

def edges(ch):
    out, prev = [], None
    for row in rows:
        v = row[1 + ch]
        if prev is not None and v != prev: out.append((row[0], v))
        prev = v
    return out

e0, e2 = edges(0), edges(2)
rises = [t for t, v in e0 if v == 1]
pulses = []
for i, (t, v) in enumerate(e0):
    if v == 1:
        fall = next((tt for tt, vv in e0[i + 1:] if vv == 0), None)
        pulses.append((t, None if fall is None else fall - t))
print(f"capture span {span:.3f} s, ch0 rows {len(rows)}")
print(f"ch0 pulses: {len(pulses)}")
for t, w in pulses: print(f"  rise t={t:.4f} s  width={'open' if w is None else f'{w*1e3:.3f} ms'}")

# T1
early = [t for t in rises if t < 20.0]
print(f"T1 rising edges in first 20 s: {len(early)} -> {'PASS' if not early else 'FAIL'}")
# T2
ws = [w for _, w in pulses if w is not None]
okw = pulses and all(w is not None and 0.045 <= w <= 0.055 for _, w in pulses)
if ws: print(f"T2 widths min/median/max {min(ws)*1e3:.3f}/{statistics.median(ws)*1e3:.3f}/{max(ws)*1e3:.3f} ms -> {'PASS' if okw else 'FAIL'}")
else: print("T2 no pulses -> FAIL")
# T5: delay from last SCL edge before each rise
scl_t = [t for t, _ in e2]
import bisect
for t, _ in pulses:
    k = bisect.bisect_left(scl_t, t) - 1
    d = t - scl_t[k] if k >= 0 else None
    print(f"T5 rise {t:.4f}: last SCL edge {('%.3f ms before' % (d*1e3)) if d is not None else 'none'} -> {'PASS' if d is not None and d <= 0.005 else 'FAIL'}")
# T7: I2C bursts = SCL edges separated by > 2 ms idle
starts = [scl_t[0]] + [scl_t[i] for i in range(1, len(scl_t)) if scl_t[i] - scl_t[i - 1] > 0.002]
ends = [scl_t[i - 1] for i in range(1, len(scl_t)) if scl_t[i] - scl_t[i - 1] > 0.002] + [scl_t[-1]]
per = [starts[i + 1] - starts[i] for i in range(len(starts) - 1)]
idle = [starts[i + 1] - ends[i] for i in range(len(starts) - 1)]
dur = [e - s for s, e in zip(starts, ends)]
print(f"T7 bursts {len(starts)}; start-to-start median {statistics.median(per)*1e3:.2f} ms "
      f"(min {min(per)*1e3:.2f}, max {max(per)*1e3:.2f}); idle gap median {statistics.median(idle)*1e3:.2f} ms; "
      f"burst length median {statistics.median(dur)*1e3:.3f} ms -> {'PASS' if 0.040 <= statistics.median(per) <= 0.050 else 'FAIL'}")
# windowed: around pulses
for t, _ in pulses:
    near = [p for s, p in zip(starts, per) if t - 0.5 < s < t + 3.5]
    if near: print(f"   bursts within -0.5..+3.5 s of {t:.2f}: median {statistics.median(near)*1e3:.2f} ms max {max(near)*1e3:.2f} ms")

def load(p):
    out = []
    for ln in open(p, errors="replace"):
        if ln.startswith("#"): continue
        a, _, b = ln.rstrip("\n").partition("\t")
        try: out.append((float(a) - t0, b))
        except ValueError: pass
    return out

# T6 (substituted): counselor log unavailable; use the tag's own log.
tag = load(taglog)
imp = [(t, l) for t, l in tag if "IMPACT" in l]
sosq = [(t, l) for t, l in tag if "SOS queue result" in l]
ack = [(t, l) for t, l in tag if "ACK tag=T01 type=3" in l]
for name, xs in (("IMPACT", imp), ("SOS queue", sosq), ("ACK tag=T01 type=3", ack)):
    print(f"tag '{name}' lines: {len(xs)}")
    for t, l in xs: print(f"  host+{t:.3f}  {l}")
for t, l in imp:
    q = [(x, ll) for x, ll in sosq if -0.05 <= x - t <= 2.0]
    a = [x for x, _ in ack if 0 <= x - t <= 2.0]
    q0 = bool(q) and "result=0" in q[0][1]
    print(f"T6 impact host+{t:.3f}: queue {'result=0' if q0 else 'MISSING/!=0'}, "
          f"ACK {'%.3f s later' % (a[0]-t) if a else 'NONE within 2 s'} -> {'PASS' if q0 and a else 'FAIL'}")
