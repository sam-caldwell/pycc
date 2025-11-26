# demos/e2e_time.py
def main() -> int:
    import time, io
    ok = True
    # Basic monotonicity checks
    t1 = time.time()
    t2 = time.time()
    if not (t2 >= t1 and t1 > 0.0):
        ok = False
    n = time.time_ns()
    if not (n > 0):
        ok = False
    m1 = time.monotonic()
    time.sleep(0.005)
    m2 = time.monotonic()
    if not (m2 - m1 >= 0.004):
        ok = False
    p1 = time.perf_counter()
    p2 = time.perf_counter()
    if not (p2 >= p1):
        ok = False
    io.write_stdout('TIME_OK\n' if ok else 'TIME_BAD\n')
    return 0 if ok else 1

