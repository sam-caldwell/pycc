def main() -> int:
    import errno, io
    # Fetch a few canonical error codes; ensure they are positive integers
    e1 = errno.EPERM()
    e2 = errno.ENOENT()
    e3 = errno.EEXIST()
    e4 = errno.EISDIR()
    e5 = errno.ENOTDIR()
    e6 = errno.EACCES()
    ok = (e1 > 0) and (e2 > 0) and (e3 > 0) and (e4 > 0) and (e5 > 0) and (e6 > 0)
    io.write_stdout('ERRNO_OK\n' if ok else 'ERRNO_BAD\n')
    return 0 if ok else 1
