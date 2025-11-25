# demos/e2e_random.py
def main() -> int:
    import random, io
    random.seed(12345)
    r = random.random()
    n = random.randint(1, 10)
    # Avoid chained comparisons (not yet supported by the compiler).
    # Compute sub-conditions separately to avoid any short-circuiting edge-cases.
    ok1 = (r >= 0.0 and r <= 1.0)
    ok2 = (n >= 1 and n <= 10)
    ok = ok1 and ok2
    if ok:
        io.write_stdout('RANDOM_OK\n')
        return 0
    else:
        io.write_stdout('RANDOM_BAD\n')
        return 1
