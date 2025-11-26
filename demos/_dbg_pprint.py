def main() -> int:
    import pprint, io
    ok = True
    s1 = pprint.pformat([1, 2, 3])
    s2 = pprint.pformat({'a': [1, 2], 'b': [3]})
    s3 = pprint.pformat("a'b\n")
    io.write_stdout('S1:'+s1+'\n')
    io.write_stdout('S2:'+s2+'\n')
    io.write_stdout('S3:'+s3+'\n')
    expect1 = "{'a': [1, 2], 'b': [3]}"
    expect2 = "{'b': [3], 'a': [1, 2]}"
    io.write_stdout('OK1:'+str(s1=='[1, 2, 3]')+'\n')
    io.write_stdout('OK2:'+str(s2==expect1 or s2==expect2)+'\n')
    io.write_stdout('OK3:'+str(s3=="'a\\'b\\\\n'")+'\n')
    return 0
