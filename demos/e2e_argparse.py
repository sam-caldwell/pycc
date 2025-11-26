# demos/e2e_argparse.py
def main() -> int:
    import argparse, pprint, io
    ok = True

    p = argparse.ArgumentParser()
    argparse.add_argument(p, '--verbose', 'store_true')
    argparse.add_argument(p, '--count', 'store_int')
    argparse.add_argument(p, '--name', 'store')

    args = ['--verbose', '--count', '3', '--name', 'bob']
    d = argparse.parse_args(p, args)

    v_str = pprint.pformat(d['verbose'])
    c_str = pprint.pformat(d['count'])
    n_val = d['name']

    if not (v_str == 'True' and c_str == '3' and n_val == 'bob'):
        ok = False

    io.write_stdout('ARGPARSE_OK\n' if ok else 'ARGPARSE_BAD\n')
    return 0 if ok else 1

