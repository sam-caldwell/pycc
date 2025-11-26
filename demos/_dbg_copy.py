def main() -> int:
    import copy, pprint, io
    inner = [2]
    orig = [1, inner, 3]
    a = copy.copy(orig)
    b = copy.deepcopy(orig)
    inner.append(99)
    s_orig = pprint.pformat(orig)
    s_a = pprint.pformat(a)
    s_b = pprint.pformat(b)
    io.write_stdout('ORIG:' + s_orig + '\n')
    io.write_stdout('A   :' + s_a + '\n')
    io.write_stdout('B   :' + s_b + '\n')
    return 0
