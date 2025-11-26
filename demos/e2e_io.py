# demos/e2e_io.py
def main() -> int:
    import io, os
    ok = True
    path = 'io_e2e.txt'

    # write_file then read_file roundtrip
    w1 = io.write_file(path, 'Hello IO!')
    r1 = io.read_file(path)
    if not (w1 and r1 == 'Hello IO!'):
        ok = False

    # stdout marker
    io.write_stdout('IO_OK\n')

    # cleanup best-effort
    os.remove(path)
    return 0 if ok else 1

