# demos/e2e_pathlib.py
def main() -> int:
    import pathlib, io
    h = pathlib.home()
    p = pathlib.join(h, 'tmp')
    base = pathlib.basename(p)
    ok1 = (base == 'tmp')
    ok2 = (pathlib.suffix('src/main.py') == '.py')
    ok = ok1 and ok2
    io.write_stdout('PATHLIB_OK\n' if ok else 'PATHLIB_BAD\n')
    return 0 if ok else 1
