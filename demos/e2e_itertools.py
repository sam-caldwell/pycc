# demos/e2e_itertools.py
def main() -> int:
    import itertools, io
    xs = [1, 2, 3]
    c2 = itertools.combinations(xs, 2)  # length 3
    p2 = itertools.permutations([1, 2], 2)  # length 2
    ok = (len(c2) == 3) and (len(p2) == 2)
    io.write_stdout('IT_OK\n' if ok else 'IT_BAD\n')
    return 0 if ok else 1

