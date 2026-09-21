"""Interactive REPL: talk to your (mock) BIOS in plain English."""
from __future__ import annotations

import argparse
import sys

from .bios_state import BiosState
from .interpreter import interpret

BANNER = """LangBIOS - natural language BIOS control (prototype)
Type things like:
  enable secure boot
  turn off virtualization
  what's my fan profile
  set fan profile to silent
  list settings
  reset to factory defaults
Type 'exit' or 'quit' to leave.
"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Talk to a mock BIOS in natural language.")
    parser.add_argument("--no-llm", action="store_true", help="disable local LLM fallback")
    parser.add_argument("command", nargs="*", help="run a single command non-interactively")
    args = parser.parse_args(argv)

    state = BiosState.load()

    if args.command:
        text = " ".join(args.command)
        result = interpret(text, state, use_llm_fallback=not args.no_llm)
        print(result.message)
        return 0 if result.ok else 1

    print(BANNER)
    while True:
        try:
            text = input("langbios> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not text:
            continue
        if text.lower() in ("exit", "quit"):
            break
        result = interpret(text, state, use_llm_fallback=not args.no_llm)
        print(result.message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
