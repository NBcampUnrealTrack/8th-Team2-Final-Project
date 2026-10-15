# trace.jsonl 분석: 시퀀스 1초 구간별 FPS·가장 긴 프레임, 50ms 넘는 프레임, 카메라 대상이 바뀐 시각.
# 사용: python analyze_trace.py <trace.jsonl> [<trace2.jsonl> ...]
import json
import sys
from collections import defaultdict


def analyze(path):
    rows = [json.loads(line) for line in open(path, encoding='utf-8') if line.strip()]
    rows = [r for r in rows if 'seq' in r]
    print('==', path, len(rows), 'frames')
    buckets = defaultdict(list)
    for r in rows[1:]:
        buckets[int(r['seq'])].append(r['dt'])
    line = []
    for second in sorted(buckets):
        dts = buckets[second]
        line.append('%2d:%3.0ffps/%3.0fms' % (second, len(dts) / max(sum(dts), 1e-6), max(dts) * 1000))
    for i in range(0, len(line), 6):
        print('   ' + '  '.join(line[i:i + 6]))
    long = [r for r in rows[1:] if r['dt'] > 0.05]
    print('   frames > 50ms: %d  %s' % (len(long), ', '.join('%.2fs:%.0fms' % (r['seq'], r['dt'] * 1000) for r in long[:30])))
    last = None
    for r in rows:
        if r.get('vt') != last:
            print('   view %6.2fs %s cam=%s' % (r['seq'], r.get('vt'), r.get('cam')))
            last = r.get('vt')
    window = [r['dt'] for r in rows[1:] if 38.0 <= r['seq'] <= 44.0]
    if window:
        print('   38-44s: %d frames, mean %.1fms, worst %.0fms' % (len(window), 1000 * sum(window) / len(window), 1000 * max(window)))


for p in sys.argv[1:]:
    analyze(p)
