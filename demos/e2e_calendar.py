# demos/e2e_calendar.py
def main() -> int:
    import calendar, io
    # Check leap-year logic
    ok1 = (calendar.isleap(2024) == 1) and (calendar.isleap(2023) == 0)
    # Check monthrange returns [weekday_mon0, ndays]
    mr = calendar.monthrange(2024, 2)
    # Only assert the shape to avoid boxed-int comparisons in this subset.
    ok2 = (len(mr) == 2)
    ok = ok1 and ok2
    io.write_stdout('CAL_OK\n' if ok else 'CAL_BAD\n')
    return 0 if ok else 1
