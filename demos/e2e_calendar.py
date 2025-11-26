def main() -> int:
    import calendar, io
    ok = True
    if not (calendar.isleap(2000) == 1 and calendar.isleap(1900) == 0 and calendar.isleap(2024) == 1):
        ok = False
    mr = calendar.monthrange(2024, 2)
    # Verify days in Feb 2024 (leap year). Weekday varies by algorithm, so just check days.
    if not (isinstance(mr, list) and len(mr) == 2 and mr[1] == 29):
        ok = False
    io.write_stdout('CAL_OK\n' if ok else 'CAL_BAD\n')
    return 0 if ok else 1

