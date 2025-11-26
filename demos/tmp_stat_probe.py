def main() -> int:
    import stat, io
    A = 1 if stat.S_ISDIR(0o040000) else 0
    B = 1 if stat.S_ISREG(0o100000) else 0
    io.write_stdout(f"A={A} B={B}\n")
    return 0
