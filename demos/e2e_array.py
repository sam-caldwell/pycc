# demos/e2e_array.py
def main() -> int:
    import array, pprint, io
    ok = True

    # int array: append and pop
    a = array.array('i', [1, 2])
    array.append(a, 3)
    p = array.pop(a)
    if pprint.pformat(p) != '3':
        ok = False
    lst = array.tolist(a)
    if pprint.pformat(lst) != '[1, 2]':
        ok = False

    io.write_stdout('ARRAY_OK\n' if ok else 'ARRAY_BAD\n')
    return 0 if ok else 1

