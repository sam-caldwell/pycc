# demos/e2e_binascii.py
def main() -> int:
    import binascii, io
    s = b'Hi'
    # Hexlify -> bytes; decode to ASCII for comparison
    hb = binascii.hexlify(s)
    h = hb.decode('ascii', 'strict')
    ok = (h == '4869')
    # Round-trip via unhexlify and hexlify again
    ub = binascii.unhexlify(h)
    hb2 = binascii.hexlify(ub)
    h2 = hb2.decode('ascii', 'strict')
    ok = ok and (h2 == '4869')
    if ok:
        io.write_stdout('BINASCII_OK\n')
        return 0
    else:
        io.write_stdout('BINASCII_BAD\n')
        return 1
