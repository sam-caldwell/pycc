def main() -> int:
    import io, reprlib
    s1 = reprlib.repr(1)
    s2 = reprlib.repr(2)
    s3 = reprlib.repr(3)
    io.write_stdout(s1 + ',' + s2 + ',' + s3 + '\n')
    return 0
