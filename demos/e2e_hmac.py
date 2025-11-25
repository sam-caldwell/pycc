# demos/e2e_hmac.py
def main() -> int:
    import hmac, binascii, io
    # Compute HMAC-SHA256 and verify hex length
    d = hmac.digest('key', 'msg', 'sha256')  # bytes
    hb = binascii.hexlify(d)
    h = hb.decode('ascii', 'strict')  # hex string length 64
    if len(h) == 64:
        io.write_stdout('HMAC_OK\n')
        return 0
    else:
        io.write_stdout('HMAC_BAD\n')
        return 1
