#!/usr/bin/env python3
"""Steady-state current of a BuddyGuard tag with a Nordic PPK2 (source mode).

    ~/ppk2-venv/bin/python tools/bg_ppk2_steady.py <mV> <settle_s> <record_s> <out.csv> [abort_mA]

Powers the board from the PPK2, waits <settle_s> seconds without recording (the tag's first
300 s after boot are the BLE maintenance window with fast connectable advertising, so use
320), records <record_s> seconds, then ALWAYS switches the output off (also on error/Ctrl-C).

Unplug the tag's USB cable first: USB would power the board around the PPK2.

The CSV holds one row per millisecond: t_ms, mean_uA, max_uA (100 raw samples each), so a
60 s record is 60,000 rows. The summary uses every raw sample. Based on closed-loop-starter's
ppk2-capture-v1.py and the same ppk2-api 0.9.2 calls.
"""
import csv
import statistics
import sys
import time

RATE = 100_000  # PPK2 samples per second
BIN = 100       # raw samples per CSV row (1 ms)


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    mv, settle, record, out = int(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
    abort_ma = float(sys.argv[5]) if len(sys.argv) > 5 else 300.0
    if not 800 <= mv <= 5000:
        sys.exit('the PPK2 sources 800 to 5000 mV')
    from ppk2_api.ppk2_api import PPK2_API  # pip install ppk2-api==0.9.2
    ports = PPK2_API.list_devices()
    if not ports:
        sys.exit('no PPK2 found')
    p = PPK2_API(ports[0], timeout=1, write_timeout=1, exclusive=True)
    p.stop_measuring()  # drain a stream left running by an interrupted run
    quiet = time.time()
    while time.time() - quiet < 0.5:
        if p.ser.in_waiting:
            p.ser.read(p.ser.in_waiting)
            quiet = time.time()
        time.sleep(0.05)
    p.get_modifiers()
    p.use_source_meter()
    p.set_source_voltage(mv)
    raw, aborted = [], None
    try:
        p.toggle_DUT_power('ON')
        p.start_measuring()
        t0 = time.time()
        last_print = 0
        while time.time() - t0 < settle + record:
            d = p.get_data()
            if d:
                s, _ = p.get_samples(d)  # 0.9.2 returns (samples in uA, digital bits)
                if len(s) > 500 and statistics.fmean(s[-5000:]) / 1000 > abort_ma:
                    aborted = f'mean {statistics.fmean(s[-5000:]) / 1000:.1f} mA > {abort_ma} mA'
                    break
                if time.time() - t0 >= settle:
                    raw.extend(s)
            elapsed = time.time() - t0
            if elapsed - last_print >= 30:
                phase = 'settling' if elapsed < settle else 'recording'
                print(f'  {elapsed:5.0f} s {phase}', flush=True)
                last_print = elapsed
            time.sleep(0.01)
    finally:
        try:
            p.stop_measuring()
        finally:
            p.toggle_DUT_power('OFF')
            p.ser.close()
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['t_ms', 'mean_uA', 'max_uA'])
        for k in range(0, len(raw) - BIN + 1, BIN):
            c = raw[k:k + BIN]
            w.writerow([k // BIN, f'{statistics.fmean(c):.1f}', f'{max(c):.1f}'])
    print(f'{mv} mV, settle {settle:.0f} s, record {record:.0f} s, port {ports[0]}, '
          f'{len(raw)} samples -> {out}, output OFF')
    if aborted:
        print('ABORTED:', aborted)
    if not raw:
        return 1
    srt = sorted(raw)
    n = len(srt)
    print(f'mean {statistics.fmean(raw) / 1000:.3f} mA  median {srt[n // 2] / 1000:.3f}  '
          f'p99 {srt[n * 99 // 100] / 1000:.3f}  max {srt[-1] / 1000:.3f} mA')
    for k in range(0, n, RATE):
        c = raw[k:k + RATE]
        print(f'  s{k // RATE:<3d} mean {statistics.fmean(c) / 1000:8.3f} mA  max {max(c) / 1000:8.3f} mA')
    return 2 if aborted else 0


if __name__ == '__main__':
    sys.exit(main())
