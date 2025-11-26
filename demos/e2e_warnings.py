def main() -> int:
    import warnings, io
    warnings.simplefilter('ignore')
    warnings.warn('oops')
    io.write_stdout('WARN_OK\n')
    return 0

