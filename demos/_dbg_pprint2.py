def main() -> int:
    import pprint, io
    s1 = pprint.pformat([1, 2, 3])
    expect1 = '[1, 2, 3]'
    if s1 == expect1:
        io.write_stdout('OK1\n')
    else:
        io.write_stdout('BAD1:' + s1 + '\n')
    s2 = pprint.pformat({'a': [1, 2], 'b': [3]})
    e1 = "{'a': [1, 2], 'b': [3]}"; e2 = "{'b': [3], 'a': [1, 2]}"
    if s2 == e1 or s2 == e2:
        io.write_stdout('OK2\n')
    else:
        io.write_stdout('BAD2:' + s2 + '\n')
    s3 = pprint.pformat("a'b\n")
    e3 = "'a\\'b\\\\n'"
    if s3 == e3:
        io.write_stdout('OK3\n')
    else:
        io.write_stdout('BAD3:' + s3 + '\n')
    return 0
