# demos/e2e_html.py
def main() -> int:
    import html, io
    a = html.escape('<&>')
    b = html.escape('\'"', 1)
    ok1 = (a == '&lt;&amp;&gt;')
    ok = ok1
    io.write_stdout('HTML_OK\n' if ok else 'HTML_BAD\n')
    return 0 if ok else 1
