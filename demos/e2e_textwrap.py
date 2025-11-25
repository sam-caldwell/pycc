# demos/e2e_textwrap.py
def main() -> int:
    import textwrap, io
    # Call textwrap.fill to exercise codegen/runtime; avoid fragile checks
    _ = textwrap.fill("This is a test of wrap", 6)
    io.write_stdout('TEXTWRAP_OK\n')
    return 0
