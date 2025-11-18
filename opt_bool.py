def main() -> int:
  a = True
  b = False
  c = (a and b) or (not b)
  return 1 if c else 0
