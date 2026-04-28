"""rumble_dash/__main__.py — Entry point: python -m rumble_dash [device]"""

import sys


def main() -> None:
    device = sys.argv[1] if len(sys.argv) > 1 else "/dev/rumble0"
    from .ui.app import run
    run(device)


if __name__ == "__main__":
    main()
