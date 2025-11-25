# demos/e2e_keyword.py
def main() -> int:
    import keyword, io
    ok1 = keyword.iskeyword('for')
    ok2 = not keyword.iskeyword('spam')
    ok = ok1 and ok2
    io.write_stdout('KEYWORD_OK\n' if ok else 'KEYWORD_BAD\n')
    return 0 if ok else 1
