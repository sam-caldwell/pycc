# demos/e2e_textwrap.py
def main() -> int:
    import textwrap, io
    s = "This is a test of wrap"
    a = textwrap.fill(s, 6)
    ok = True
    # Positive membership checks for expected segments
    if not ("This" in a):
        ok = False
    if ok and not ("is a" in a):
        ok = False
    if ok and not ("test" in a):
        ok = False
    if ok and not ("of" in a):
        ok = False
    if ok and not ("wrap" in a):
        ok = False
    # Negative membership checks ensure breaks are not spaces around line boundaries
    if ok and ("This " in a):
        ok = False
    if ok and (" is" in a):
        ok = False
    if ok and (" test" in a):
        ok = False
    if ok and (" of" in a):
        ok = False
    if ok and (" wrap" in a):
        ok = False
    io.write_stdout('TEXTWRAP_OK\n' if ok else 'TEXTWRAP_BAD\n')
    return 0 if ok else 1
