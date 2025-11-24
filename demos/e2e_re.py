# demos/e2e_re.py
def main() -> int:
    import re, io
    # search returns a match object (opaque); compare to None to get bool
    ok1 = (re.search('world', 'hello world') != None)
    s = re.sub('a+', 'b', 'caa')  # -> 'cb'
    ok2 = (s == 'cb')
    ok = ok1 and ok2
    io.write_stdout('RE_OK\n' if ok else 'RE_BAD\n')
    return 0 if ok else 1

