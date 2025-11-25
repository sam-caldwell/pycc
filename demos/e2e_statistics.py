# demos/e2e_statistics.py
def main() -> int:
    import statistics, io
    ok = True
    m = statistics.mean([1, 2, 3])
    if not (m == 2.0):
        ok = False
    if ok:
        med = statistics.median([1, 2, 3, 4])
        if not (med == 2.5):
            ok = False
    if ok:
        sd = statistics.stdev([1, 2, 3])
        if not (sd == 1.0):
            ok = False
    if ok:
        pv = statistics.pvariance([1, 2, 3])
        # 2/3 ~= 0.666..., tolerate small error
        diff = pv - 0.6666666666667
        if diff < 0.0:
            diff = -diff
        if not (diff < 1e-6):
            ok = False
    io.write_stdout('STATISTICS_OK\n' if ok else 'STATISTICS_BAD\n')
    return 0 if ok else 1
