# demos/e2e_json.py
def main() -> int:
    import json, io
    # Roundtrip a small dict/list structure
    obj = {'a': 1, 'b': [2, 3]}
    s1 = json.dumps(obj)
    obj2 = json.loads(s1)
    s2 = json.dumps(obj2)
    ok = ('"a"' in s2) and ('3' in s2)
    io.write_stdout('JSON_OK\n' if ok else 'JSON_BAD\n')
    return 0 if ok else 1

