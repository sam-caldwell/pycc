# demos/e2e_types.py
def main() -> int:
    import types, io, reprlib
    ok = True
    ns = types.SimpleNamespace([['name', 'bob'], ['age', 3]])
    # Access attributes
    if not (ns.name == 'bob'):
        ok = False
    if not (reprlib.repr(ns.age) == '3'):
        ok = False
    # Empty namespace
    ns2 = types.SimpleNamespace()
    # Should support setting new attributes (store via object attr at runtime);
    # not tested here to keep within subset
    io.write_stdout('TYPES_OK\n' if ok else 'TYPES_BAD\n')
    return 0 if ok else 1

