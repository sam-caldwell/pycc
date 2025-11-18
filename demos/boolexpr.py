# demos/boolexpr.py
def main() -> int:
    a = True
    b = False
    c = (a and b) or (not b)
    if c:
        return 1
    else:
        return 0

