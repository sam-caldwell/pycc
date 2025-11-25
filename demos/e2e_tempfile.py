# demos/e2e_tempfile.py
def main() -> int:
    import tempfile, io
    ok = True
    d = tempfile.gettempdir()
    if not (len(d) > 0):
        ok = False
    if ok:
        p = tempfile.mkdtemp()
        if not (len(p) > len(d)):
            ok = False
    if ok:
        pair = tempfile.mkstemp()
        # In this subset, mkstemp returns [fd:int, path:str]. We only assert shape here
        # to avoid over-constraining element typing in AOT.
        if not (len(pair) == 2):
            ok = False
    io.write_stdout('TEMPFILE_OK\n' if ok else 'TEMPFILE_BAD\n')
    return 0 if ok else 1
