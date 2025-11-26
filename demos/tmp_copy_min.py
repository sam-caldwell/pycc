def main() -> int:
    import copy, io
    a = [1, [2], 3]
    b = copy.copy(a)
    b[1].append(9)
    io.write_stdout('OK\n')
    return 0
