# demos/e2e_base64.py
def main() -> int:
    import base64, io
    s = b'Hello, World!'
    enc = base64.b64encode(s)
    dec = base64.b64decode(enc)
    ok = (dec.decode('ascii', 'strict') == 'Hello, World!')
    # Also exercise str input
    enc2 = base64.b64encode('Hi')
    dec2 = base64.b64decode(enc2)
    ok = ok and (dec2.decode('ascii', 'strict') == 'Hi')
    if ok:
        io.write_stdout('BASE64_OK\n')
        return 0
    else:
        io.write_stdout('BASE64_BAD\n')
        return 1
