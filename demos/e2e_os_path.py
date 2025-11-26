# demos/e2e_os_path.py
def main() -> int:
    import os, io
    ok = True

    import posixpath
    # join/dirname/basename/splitext/abspath shapes (posixpath)
    j = posixpath.join('a', 'b')
    d = posixpath.dirname('/tmp/x')
    b = posixpath.basename('/tmp/x')
    root_ext = posixpath.splitext('/tmp/x.txt')
    ap = posixpath.abspath('.')
    if not (len(j) > 0 and len(d) > 0 and b == 'x' and len(ap) > 0 and len(root_ext) == 2):
        ok = False

    io.write_stdout('OSPATH_OK\n' if ok else 'OSPATH_BAD\n')
    return 0 if ok else 1
