# demos/loops.py
def main() -> int:
    s = 0
    for x in [1, 2, 3]:
        s = s + x
    i = 0
    while i < 2:
        s = s + 1
        i = i + 1
    return s

