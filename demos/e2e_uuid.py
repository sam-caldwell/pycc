# demos/e2e_uuid.py
def main() -> int:
    import uuid, io, re
    s = uuid.uuid4()  # returns string in this subset
    # Validate canonical UUIDv4 format
    ok_len = (len(s) == 36)
    ok_re = (re.search('^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$', s) != None)
    ok = ok_len and ok_re
    io.write_stdout('UUID_OK\n' if ok else 'UUID_BAD\n')
    return 0 if ok else 1
