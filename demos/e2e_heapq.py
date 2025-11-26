def main() -> int:
    import heapq, io, reprlib
    a = []
    heapq.heappush(a, 3)
    heapq.heappush(a, 1)
    heapq.heappush(a, 2)
    x = reprlib.repr(heapq.heappop(a))
    y = reprlib.repr(heapq.heappop(a))
    z = reprlib.repr(heapq.heappop(a))
    ok = (x == '1') and (y == '2') and (z == '3')
    io.write_stdout('HEAPQ_OK\n' if ok else 'HEAPQ_BAD\n')
    return 0 if ok else 1
