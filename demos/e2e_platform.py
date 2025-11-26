def main() -> int:
    import platform, io
    sysn = platform.system()
    mach = platform.machine()
    rel = platform.release()
    ver = platform.version()
    ok = (len(sysn) > 0) and (len(mach) > 0) and (len(rel) > 0) and (len(ver) > 0)
    io.write_stdout('PLATFORM_OK\n' if ok else 'PLATFORM_BAD\n')
    return 0 if ok else 1

