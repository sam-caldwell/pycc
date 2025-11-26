# demos/e2e_collections.py
def main() -> int:
    import collections, io
    ok = True

    # Counter over list
    data = ['a', 'b', 'a']
    cnt = collections.Counter(data)

    # Verify counts via string form to avoid numeric compare boxing
    import pprint
    a_count = pprint.pformat(cnt['a'])
    b_count = pprint.pformat(cnt['b'])
    if not (a_count == '2' and b_count == '1'):
        ok = False

    # OrderedDict from list of pairs; verify lookup
    pairs = [['x', '1'], ['y', '2']]
    od = collections.OrderedDict(pairs)
    if not (od['x'] == '1' and od['y'] == '2'):
        ok = False

    # defaultdict behavior
    dd = collections.defaultdict('x')
    d0 = collections.defaultdict_get(dd, 'missing')
    if not (d0 == 'x'):
        ok = False

    io.write_stdout('COLLECTIONS_OK\n' if ok else 'COLLECTIONS_BAD\n')
    return 0 if ok else 1
