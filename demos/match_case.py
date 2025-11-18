# demos/match_case.py
def main() -> int:
    x = 2
    match x:
        case 1:
            return 1
        case _:
            return 0

