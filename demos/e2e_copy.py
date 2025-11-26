def main() -> int:
    import copy, pprint, io
    inner = [2]
    orig = [1, inner, 3]
    a = copy.copy(orig)
    b = copy.deepcopy(orig)
    # mutate the original inner list; shallow copy shares, deep copy does not
    inner.append(99)
    s_orig = pprint.pformat(orig)
    s_a = pprint.pformat(a)
    s_b = pprint.pformat(b)
    ok = (s_orig == '[1, [2, 99], 3]') and (s_a == '[1, [2, 99], 3]') and (s_b == '[1, [2], 3]')
    io.write_stdout('COPY_OK\n' if ok else 'COPY_BAD\n')
    return 0 if ok else 1
