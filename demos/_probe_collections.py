# probe
def main() -> int:
    import collections, io, pprint
    ok = True
    data = ['a','b','a']
    cnt = collections.Counter(data)
    a_count = pprint.pformat(cnt['a'])
    b_count = pprint.pformat(cnt['b'])
    io.write_stdout('AOK\n' if (a_count == '2') else 'ABAD\n')
    io.write_stdout('BOK\n' if (b_count == '1') else 'BBAD\n')
    if not (a_count == '2' and b_count == '1'):
        ok = False
    pairs = [['x','1'],['y','2']]
    od = collections.OrderedDict(pairs)
    io.write_stdout('XOK\n' if (od['x'] == '1') else 'XBAD\n')
    io.write_stdout('YOK\n' if (od['y'] == '2') else 'YBAD\n')
    if not (od['x'] == '1' and od['y'] == '2'):
        ok = False
    # skip ChainMap in probe
    dd = collections.defaultdict('x')
    d0 = collections.defaultdict_get(dd, 'missing')
    collections.defaultdict_set(dd, 'missing', 'y')
    d1 = collections.defaultdict_get(dd, 'missing')
    io.write_stdout('D0OK\n' if (d0 == 'x') else 'D0BAD\n')
    io.write_stdout('D1OK\n' if (d1 == 'y') else 'D1BAD\n')
    if not (d0 == 'x' and d1 == 'y'):
        ok = False
    io.write_stdout('OK1\n' if ok else 'OK0\n')
    return 0 if ok else 1
