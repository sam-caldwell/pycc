# demos/e2e_binascii.py
def main() -> int:
    import binascii, io
    s = b'Hi'
    # Hexlify -> bytes; decode to ASCII for comparison
    h = binascii.hexlify(s).decode('ascii', 'strict')
    ok = (h == '4869')
    # Round-trip via unhexlify and hexlify again
    h2 = binascii.hexlify(binascii.unhexlify(h)).decode('ascii', 'strict')
    ok = ok and (h2 == '4869')
    if ok:
        io.write_stdout('BINASCII_OK\n')
        return 0
    else:
        io.write_stdout('BINASCII_BAD\n')
        return 1
