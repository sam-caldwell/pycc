# demos/e2e_getpass.py
def main() -> int:
    import getpass, io
    u = getpass.getuser()
    p1 = getpass.getpass('pwd:')
    p2 = getpass.getpass()
    ok = (len(u) > 0) and (p1 == '') and (p2 == '')
    io.write_stdout('GETPASS_OK\n' if ok else 'GETPASS_BAD\n')
    return 0 if ok else 1
