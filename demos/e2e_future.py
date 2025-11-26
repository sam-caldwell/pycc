# demos/e2e_future.py
def main() -> int:
    import __future__, io
    ok = True
    a = __future__.annotations()
    if not a:
        ok = False
    # Unknown features return False in this subset
    u = __future__.unicode_literals()
    if u:
        ok = False
    io.write_stdout('FUTURE_OK\n' if ok else 'FUTURE_BAD\n')
    return 0 if ok else 1

