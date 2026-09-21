# LangBIOS

A prototype for interacting with BIOS/UEFI settings in natural language.

Since a running OS can't portably read/write real firmware settings, this
prototype simulates a machine's BIOS as a small JSON-backed state store
(`langbios/bios_state.json`) with realistic settings: secure boot, TPM,
virtualization, fast boot, XMP, CPU turbo, power profile, fan profile, and
boot order.

## How it understands you

Two layers, tried in order:

1. **Rule-based parser** (`langbios/rule_parser.py`) — fast, free, exact
   keyword/regex matching for common phrasings ("enable secure boot",
   "what's my fan profile", "list settings"). Returns nothing if unsure.
2. **Local LLM fallback** (`langbios/llm_parser.py`) — for anything the
   rules miss, LangBIOS asks a local model served by
   [Ollama](https://ollama.com) to map the request to a structured
   command. No API keys, no cloud calls, fully offline once the model is
   pulled.

If neither layer can confidently interpret the request, LangBIOS says so
instead of guessing.

## Setup

```bash
pip install -r requirements-dev.txt

# optional, for the LLM fallback:
# 1. install Ollama: https://ollama.com/download
# 2. ollama pull llama3.2   (or any small open-source model you prefer)
# 3. ollama serve
```

## Usage

Interactive:

```bash
python -m langbios.cli
```

One-shot:

```bash
python -m langbios.cli "enable secure boot"
python -m langbios.cli "set fan profile to silent"
python -m langbios.cli "list settings"
python -m langbios.cli --no-llm "reset to factory defaults"
```

`--no-llm` disables the local-LLM fallback (rule-based matching only).

Configure the LLM fallback via environment variables:

- `LANGBIOS_OLLAMA_URL` (default `http://localhost:11434`)
- `LANGBIOS_OLLAMA_MODEL` (default `llama3.2`)
- `LANGBIOS_OLLAMA_TIMEOUT` (default `10` seconds)

## Tests

```bash
python -m pytest
```
