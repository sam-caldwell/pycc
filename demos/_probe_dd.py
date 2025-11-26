# probe

def main() -> int:
    import collections, io
    dd = collections.defaultdict('x')
    d0 = collections.defaultdict_get(dd, 'missing')
    collections.defaultdict_set(dd, 'missing', 'y')
    d1 = collections.defaultdict_get(dd, 'missing')
    io.write_stdout('Z0\n' if (d0 == 'x') else 'Z0BAD\n')
    io.write_stdout('Z1\n' if (d1 == 'y') else 'Z1BAD\n')
    return 0
