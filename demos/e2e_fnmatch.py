# demos/e2e_fnmatch.py
def main() -> int:
    import fnmatch, io
    names = ['a.py', 'b.txt', 'c.py']
    m = fnmatch.filter(names, '*.py')
    if len(m) == 2 and fnmatch.fnmatch('file.txt', 'file*.txt') and fnmatch.fnmatchcase('a.c', 'a.?'):
        io.write_stdout('FN_OK\n')
    else:
        io.write_stdout('FN_BAD\n')
    return len(m)

