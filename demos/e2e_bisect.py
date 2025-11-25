# demos/e2e_bisect.py
def main() -> int:
    import bisect, io
    lst = [1, 2, 2, 3]
    i1 = bisect.bisect_left(lst, 2)
    i2 = bisect.bisect_right(lst, 2)
    i3 = bisect.bisect(lst, 3)
    ok = (i1 == 1) and (i2 == 3) and (i3 == 4)
    # Test insort (alias to insort_right) and insort_left
    if ok:
        bisect.insort(lst, 2)
        j2 = bisect.bisect_right(lst, 2)
        ok = (j2 == 4)
    if ok:
        bisect.insort_left(lst, 2)
        k1 = bisect.bisect_left(lst, 2)
        k2 = bisect.bisect_right(lst, 2)
        ok = (k1 == 1) and (k2 == 5)
    io.write_stdout('BISECT_OK\n' if ok else 'BISECT_BAD\n')
    return 0 if ok else 1
