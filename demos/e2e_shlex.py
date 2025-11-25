# demos/e2e_shlex.py
def main() -> int:
    import shlex, io
    toks = shlex.split("a 'b c'")
    j = shlex.join(toks)
    ok = (len(toks) == 2) and (j == "a 'b c'")
    io.write_stdout('SHLEX_OK\n' if ok else 'SHLEX_BAD\n')
    return 0 if ok else 1
