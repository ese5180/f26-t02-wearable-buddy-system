"""LoRa TX events in a bg_ppk2_steady CSV: 50 ms moving average of the 1 ms bin means, events where it
exceeds median + delta_mA (default 15), merged within 1 s. Prints times, widths, intervals.
    python peaks2.py <csv> [delta_mA]"""
import csv, statistics, sys
m=[float(r['mean_uA'])/1000 for r in csv.DictReader(open(sys.argv[1]))]
W=50; d=float(sys.argv[2]) if len(sys.argv)>2 else 15
cs=[0]
for v in m: cs.append(cs[-1]+v)
ma=[(cs[i+W]-cs[i])/W for i in range(len(m)-W+1)]
med=statistics.median(ma); thr=med+d
ev=[];last=-10**6
for i,v in enumerate(ma):
    if v>thr:
        if i-last>1000: ev.append([i,i,v])
        else: ev[-1][1]=i; ev[-1][2]=max(ev[-1][2],v)
        last=i
print(f'50 ms MA median {med:.2f} mA, max {max(ma):.2f} mA, threshold {thr:.2f} mA, {len(ev)} events')
for a,b,p in ev: print(f'  {a/1000:7.3f} s  width {b-a+W} ms  peak 50 ms mean {p:.1f} mA')
iv=[(y[0]-x[0])/1000 for x,y in zip(ev,ev[1:])]
print('intervals s:',[round(x,2) for x in iv])
if iv: print(f'min {min(iv):.2f} max {max(iv):.2f} s; all 10 +/- 1 s: {all(9<=x<=11 for x in iv)}')
