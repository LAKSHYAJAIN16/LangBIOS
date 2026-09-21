import urllib.error

from langbios import llm_parser
from langbios.bios_state import BiosState
from langbios.interpreter import interpret


def test_rule_path_does_not_touch_llm(tmp_path, monkeypatch):
    def boom(*a, **kw):
        raise AssertionError("LLM should not be called when rules match")

    monkeypatch.setattr(llm_parser, "parse", boom)

    state = BiosState.load(tmp_path / "state.json")
    result = interpret("enable secure boot", state)
    assert result.ok
    assert state.get("secure_boot") is True


def test_llm_fallback_used_when_rules_miss(tmp_path, monkeypatch):
    from langbios.commands import Command

    monkeypatch.setattr(
        llm_parser, "parse", lambda text: Command(action="set", setting="tpm", value=False)
    )

    state = BiosState.load(tmp_path / "state.json")
    result = interpret("kill the trusted platform thingy", state)
    assert result.ok
    assert state.get("tpm") is False


def test_llm_unavailable_reports_clearly(tmp_path, monkeypatch):
    def raise_unavailable(text):
        raise llm_parser.LLMUnavailableError("connection refused")

    monkeypatch.setattr(llm_parser, "parse", raise_unavailable)

    state = BiosState.load(tmp_path / "state.json")
    result = interpret("do something totally unrecognized", state)
    assert not result.ok
    assert "LLM fallback" in result.message


def test_no_llm_flag_skips_fallback(tmp_path, monkeypatch):
    def boom(*a, **kw):
        raise AssertionError("LLM should not be called when disabled")

    monkeypatch.setattr(llm_parser, "parse", boom)

    state = BiosState.load(tmp_path / "state.json")
    result = interpret("do something totally unrecognized", state, use_llm_fallback=False)
    assert not result.ok
