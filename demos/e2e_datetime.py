# demos/e2e_datetime.py
def main() -> int:
    import datetime, io, time
    ok = True

    # Basic shape check: YYYY-MM-DDTHH:MM:SS
    # Implemented inline to avoid nested defs (not supported in this subset).

    n = datetime.now()
    u = datetime.utcnow()
    n_ok = (len(n) >= 19)
    u_ok = (len(u) >= 19)
    if not (n_ok and u_ok):
        ok = False

    # Epoch cases: utcfromtimestamp should be exactly the epoch in UTC
    u0 = datetime.utcfromtimestamp(0)
    if u0 != '1970-01-01T00:00:00':
        ok = False

    # fromtimestamp is local time; only verify shape
    l0 = datetime.fromtimestamp(0)
    if not (len(l0) >= 19):
        ok = False

    # Monotonic sanity check around now()
    t1 = time.time()
    t2 = time.time()
    if not (t2 >= t1):
        ok = False

    io.write_stdout('DATETIME_OK\n' if ok else 'DATETIME_BAD\n')
    return 0 if ok else 1
