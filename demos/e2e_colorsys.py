# demos/e2e_colorsys.py
def main() -> int:
    import colorsys, pprint, io
    ok = True

    # Red to HSV
    hsv = colorsys.rgb_to_hsv(1.0, 0.0, 0.0)
    if pprint.pformat(hsv) != '[0.0, 1.0, 1.0]':
        ok = False

    # HSV to RGB
    rgb = colorsys.hsv_to_rgb(0.0, 1.0, 1.0)
    if pprint.pformat(rgb) != '[1.0, 0.0, 0.0]':
        ok = False

    io.write_stdout('COLORSYS_OK\n' if ok else 'COLORSYS_BAD\n')
    return 0 if ok else 1

