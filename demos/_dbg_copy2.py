def main() -> int:
    import copy, pprint, io
    inner = [2]
    orig = [1, inner, 3]
    a = copy.copy(orig)
    b = copy.deepcopy(orig)
    inner.append(99)
    io.write_stdout(pprint.pformat(orig)); io.write_stdout('\n')
    io.write_stdout(pprint.pformat(a)); io.write_stdout('\n')
    io.write_stdout(pprint.pformat(b)); io.write_stdout('\n')
    return 0
