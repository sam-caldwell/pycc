# demos/e2e_unicodedata.py
def main() -> int:
    import unicodedata, io
    # In this subset (without ICU), normalization returns a copy of the input
    s1 = unicodedata.normalize('NFC', 'cafe')
    s2 = unicodedata.normalize('NFD', 'cafe')
    s3 = unicodedata.normalize('NFKC', 'cafe')
    s4 = unicodedata.normalize('NFKD', 'cafe')
    ok = (s1 == 'cafe') and (s2 == 'cafe') and (s3 == 'cafe') and (s4 == 'cafe')
    io.write_stdout('UNICODE_OK\n' if ok else 'UNICODE_BAD\n')
    return 0 if ok else 1
