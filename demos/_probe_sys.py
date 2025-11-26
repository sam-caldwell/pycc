# probe sys

def main() -> int:
    import sys, io
    ok = True
    p = sys.platform()
    io.write_stdout('P1\n' if (len(p) > 0) else 'P0\n')
    v = sys.version()
    io.write_stdout('V1\n' if (len(v) > 0) else 'V0\n')
    m = sys.maxsize()
    io.write_stdout('M1\n' if (m > 0) else 'M0\n')
    sys.exit(0)
    return 0
