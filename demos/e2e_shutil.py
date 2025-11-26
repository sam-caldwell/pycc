# demos/e2e_shutil.py
def main() -> int:
    import io, os, shutil
    base = '_shutil_e2e'
    src = base + '/src.txt'
    dst1 = base + '/dst1.txt'
    dst2 = base + '/dst2.txt'
    # setup
    os.mkdir(base)
    io.write_file(src, 'Hello Shutil!')
    ok = True
    # copyfile returns bool in this subset
    ok = ok and shutil.copyfile(src, dst1)
    ok = ok and shutil.copy(src, dst2)
    # verify contents
    c1 = io.read_file(dst1)
    c2 = io.read_file(dst2)
    ok = ok and (c1 == 'Hello Shutil!') and (c2 == 'Hello Shutil!')
    # negative case: copying missing file returns False
    ok = ok and (not shutil.copyfile(base + '/missing.txt', base + '/nowhere.txt'))
    # report
    if ok:
        io.write_stdout('SHUTIL_OK\n')
        rc = 0
    else:
        io.write_stdout('SHUTIL_BAD\n')
        rc = 1
    # cleanup (best-effort)
    os.remove(src)
    os.remove(dst1)
    os.remove(dst2)
    os.remove(base)
    return rc
