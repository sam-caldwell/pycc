def main() -> int:
    import pprint, io
    s1 = pprint.pformat([1,2,3])
    s2 = pprint.pformat({'a':[1,2], 'b':[3]})
    s3 = pprint.pformat("a'b\n")
    io.write_stdout(s1)
    io.write_stdout('\n')
    io.write_stdout(s2)
    io.write_stdout('\n')
    io.write_stdout(s3)
    io.write_stdout('\n')
    return 0
