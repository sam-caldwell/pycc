# demos/e2e_hashlib.py
def main() -> int:
    import hashlib, io
    h1 = hashlib.sha256('hello')
    h2 = hashlib.sha256('world')
    m = hashlib.md5(b'hello')
    ok = (len(h1) == 64 and len(h2) == 64 and len(m) == 32 and h1 != h2)
    if ok:
        io.write_stdout('HASHLIB_OK' + '\n')
        return 0
    else:
        io.write_stdout('HASHLIB_BAD' + '\n')
        return 1

