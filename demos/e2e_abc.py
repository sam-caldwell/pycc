# demos/e2e_abc.py
def main() -> int:
    import _abc, io
    ok = True
    _abc.reset()
    t0 = _abc.get_cache_token()
    # Use stable string objects for identity-based registry
    A = 'A'
    B = 'B'
    # First registration returns True; second returns False
    first = _abc.register(A, B)
    again = _abc.register(A, B)
    if not (first and (not again)):
        ok = False
    if not _abc.is_registered(A, B):
        ok = False
    _abc.invalidate_cache()
    t1 = _abc.get_cache_token()
    if not (t1 > t0):
        ok = False
    _abc.reset()
    if not (_abc.get_cache_token() == 0 and (not _abc.is_registered(A, B))):
        ok = False
    io.write_stdout('ABC_OK\n' if ok else 'ABC_BAD\n')
    return 0 if ok else 1
