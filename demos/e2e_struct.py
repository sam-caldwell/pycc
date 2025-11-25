# demos/e2e_struct.py
def main() -> int:
    import struct, binascii, io
    # Single int: size and round-trip stability (via hex length)
    b = struct.pack('<i', [123])
    hlen1 = len(binascii.hexlify(b))
    ok1 = (hlen1 == 8)
    l = struct.unpack('<i', b)
    b_round = struct.pack('<i', l)
    hlen2 = len(binascii.hexlify(b_round))
    ok3 = (hlen2 == hlen1)
    # Two ints: size and round-trip stability
    b2 = struct.pack('<ii', [1, 2])
    hlen3 = len(binascii.hexlify(b2))
    ok4 = (hlen3 == 16)
    l2 = struct.unpack('<ii', b2)
    b2_round = struct.pack('<ii', l2)
    hlen4 = len(binascii.hexlify(b2_round))
    ok6 = (hlen4 == hlen3)
    # Calcsize
    n = struct.calcsize('<ii')
    ok7 = (n == 8)
    score = 0
    if ok1:
        score = score + 1
    if ok3:
        score = score + 1
    if ok4:
        score = score + 1
    if ok6:
        score = score + 1
    if ok7:
        score = score + 1
    ok = (score == 5)
    io.write_stdout('STRUCT_OK\n' if ok else 'STRUCT_BAD\n')
    return 0 if ok else 1
