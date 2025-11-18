# demos/try_except.py
def main() -> int:
    x = 0
    try:
        x = 1
    except Exception as e:
        x = 2
    else:
        x = x + 1
    finally:
        y = 4
    return x

