# demos/comprehensions.py
def main() -> int:
    a = [x for x in [1,2,3] if True]
    b = {x for x in [1,2,3]}
    c = {x: x for x in [1,2,3]}
    return len(a)

