# demos/e2e_stat.py
def main() -> int:
    import stat, io
    # Use POSIX-style mode bits to test predicates
    # Use decimal equivalents to avoid non-decimal literal parsing issues
    mode_dir = 16877  # 0o040755 directory
    mode_reg = 33188  # 0o100644 regular file
    ok1 = stat.S_ISDIR(mode_dir) and (not stat.S_ISREG(mode_dir))
    ok2 = stat.S_ISREG(mode_reg) and (not stat.S_ISDIR(mode_reg))
    ok = ok1 and ok2
    io.write_stdout('STAT_OK\n' if ok else 'STAT_BAD\n')
    return 0 if ok else 1
