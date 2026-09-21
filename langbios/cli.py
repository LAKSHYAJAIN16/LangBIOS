"""Interactive REPL: talk to your BIOS in plain English.

Talks to actual firmware via native/build/langbios_native.{dll,so}
(build it first with native/build.ps1 or native/build.sh). Real UEFI
reads/writes need this process to be elevated; see the README.
"""
from __future__ import annotations

import argparse
import sys

from .interpreter import interpret

BANNER = """LangBIOS - natural language BIOS control (REAL firmware mode)
This talks to actual UEFI/BIOS state on this machine. Reads/writes to
Secure Boot and boot order need this process running elevated; other
settings only work on hardware with a vendor BIOS management stack
installed (Dell/HP/Lenovo). See README.md for details.
Type 'exit' or 'quit' to leave.
"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Talk to your BIOS in natural language.")
    parser.add_argument("--no-llm", action="store_true", help="disable local LLM fallback")
    parser.add_argument("command", nargs="*", help="run a single command non-interactively")
    args = parser.parse_args(argv)

    def run(text: str):
        return interpret(text, use_llm_fallback=not args.no_llm)

    if args.command:
        result = run(" ".join(args.command))
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
        result = run(text)
        print(result.message)
    return 0


if __name__ == "__main__":
    sys.exit(main())
