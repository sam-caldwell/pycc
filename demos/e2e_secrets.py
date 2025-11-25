# demos/e2e_secrets.py
def main() -> int:
    import secrets, io, re, binascii
    # Check token_bytes by hexlify length to avoid bytes len tag inference
    hb = binascii.hexlify(secrets.token_bytes(16))  # bytes of length 32
    th = secrets.token_hex(8)  # string of length 16
    tu = secrets.token_urlsafe(8)  # urlsafe base64 string (no padding)
    ok = (len(hb) == 32) and (len(th) == 16)
    ok = ok and (len(tu) > 0)
    ok_charset = (re.search('^[A-Za-z0-9_-]+$', tu) != None)
    ok = ok and ok_charset
    if ok:
        io.write_stdout('SECRETS_OK\n')
        return 0
    else:
        io.write_stdout('SECRETS_BAD\n')
        return 1
