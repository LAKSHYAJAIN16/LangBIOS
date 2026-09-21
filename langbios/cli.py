"""Interactive REPL: talk to your BIOS in plain English.

Two backends:
  --mock (default): a simulated BIOS state file, safe on any machine.
  --real: talks to actual firmware via native/build/langbios_native.dll
          (build it first with native/build.ps1). Real UEFI reads/writes
          need this process to be elevated; see the README.
"""
from __future__ import annotations

import argparse
import sys

from .bios_state import BiosState
from .interpreter import interpret, interpret_real

BANNER_MOCK = """LangBIOS - natural language BIOS control (mock mode)
Type things like:
  enable secure boot
  turn off virtualization
  what's my fan profile
  set fan profile to silent
  list settings
  reset to factory defaults
Type 'exit' or 'quit' to leave.
"""

BANNER_REAL = """LangBIOS - natural language BIOS control (REAL firmware mode)
This talks to actual UEFI/BIOS state on this machine. Reads/writes to
Secure Boot and boot order need this process running elevated; other
settings only work on Dell/HP/Lenovo hardware with their BIOS
management stack installed. See README.md for details.
Type 'exit' or 'quit' to leave.
"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Talk to your BIOS in natural language.")
    parser.add_argument("--no-llm", action="store_true", help="disable local LLM fallback")
    parser.add_argument("--real", action="store_true", help="talk to real firmware instead of the mock")
    parser.add_argument("command", nargs="*", help="run a single command non-interactively")
    args = parser.parse_args(argv)

    def run(text: str):
        if args.real:
            return interpret_real(text, use_llm_fallback=not args.no_llm)
        return interpret(text, state, use_llm_fallback=not args.no_llm)

    state = None if args.real else BiosState.load()

    if args.command:
        result = run(" ".join(args.command))
        print(result.message)
        return 0 if result.ok else 1

    print(BANNER_REAL if args.real else BANNER_MOCK)
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
