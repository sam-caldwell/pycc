def main() -> int:
    import heapq, io, reprlib
    a = []
    heapq.heappush(a, 3)
    heapq.heappush(a, 1)
    heapq.heappush(a, 2)
    x = reprlib.repr(heapq.heappop(a))
    y = reprlib.repr(heapq.heappop(a))
    z = reprlib.repr(heapq.heappop(a))
    io.write_stdout(x)
    io.write_stdout('\n')
    io.write_stdout(y)
    io.write_stdout('\n')
    io.write_stdout(z)
    io.write_stdout('\n')
    return 0
