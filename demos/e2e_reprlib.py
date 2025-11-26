# demos/e2e_reprlib.py
def main() -> int:
    import reprlib, io
    ok = True
    s1 = reprlib.repr([1, 2, 3])
    s2 = reprlib.repr('abc')
    if not (s1 == '[1, 2, 3]' and s2 == "'abc'"):
        ok = False
    # Long string truncation: length capped at 60
    long_s = 'abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789'
    r = reprlib.repr(long_s)
    if not (len(r) == 60):
        ok = False
    io.write_stdout('REPRLIB_OK\n' if ok else 'REPRLIB_BAD\n')
    return 0 if ok else 1
