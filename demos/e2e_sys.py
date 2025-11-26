# demos/e2e_sys.py
def main() -> int:
    import sys, io
    ok = True
    p = sys.platform()
    v = sys.version()
    m = sys.maxsize()
    if not (len(p) > 0):
        ok = False
    if not (len(v) > 0):
        ok = False
    if not (m != 0):
        ok = False
    # Ensure sys.exit does not terminate in test harness
    sys.exit(0)
    io.write_stdout('SYS_OK\n' if ok else 'SYS_BAD\n')
    return 0 if ok else 1
