# demos/e2e_glob.py
def main() -> int:
    import os, io, glob
    base = '_glob_e2e'
    os.mkdir(base)
    io.write_file(base + '/a.py', 'x')
    io.write_file(base + '/b.txt', 'y')
    io.write_file(base + '/c.py', 'z')
    m = glob.glob(base + '/*.py')
    if len(m) >= 2:
        io.write_stdout('GLOB_OK\n')
    else:
        io.write_stdout('GLOB_BAD\n')
    # cleanup (best effort)
    os.remove(base + '/a.py')
    os.remove(base + '/b.txt')
    os.remove(base + '/c.py')
    os.remove(base)
    return len(m)

