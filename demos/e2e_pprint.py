def main() -> int:
    import pprint, io
    ok = True
    # Simple list formatting
    s1 = pprint.pformat([1, 2, 3])
    if s1 != '[1, 2, 3]':
        ok = False
    # Nested structure formatting (dict of lists)
    s2 = pprint.pformat({'a': [1, 2], 'b': [3]})
    # Order of dict iteration in this runtime is stable; check one of two possible orders to be safe
    expect1 = "{'a': [1, 2], 'b': [3]}"
    expect2 = "{'b': [3], 'a': [1, 2]}"
    if not (s2 == expect1 or s2 == expect2):
        ok = False
    # String escaping correctness is covered in unit tests; keep e2e deterministic
    io.write_stdout('PPRINT_OK\n' if ok else 'PPRINT_BAD\n')
    return 0 if ok else 1
