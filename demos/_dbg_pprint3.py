def main() -> int:
    import pprint, io
    s1 = pprint.pformat([1, 2, 3])
    if s1 == '[1, 2, 3]':
        io.write_stdout('OK1\n')
    else:
        io.write_stdout('BAD1\n')
    s2 = pprint.pformat({'a': [1, 2], 'b': [3]})
    if s2 == "{'a': [1, 2], 'b': [3]}" or s2 == "{'b': [3], 'a': [1, 2]}":
        io.write_stdout('OK2\n')
    else:
        io.write_stdout('BAD2\n')
    s3 = pprint.pformat("a'b\n")
    if s3 == "'a\\'b\\\\n'":
        io.write_stdout('OK3\n')
    else:
        io.write_stdout('BAD3\n')
    return 0
