import json
import urllib.error

import pytest

from langbios import llm_parser


class _FakeResponse:
    def __init__(self, payload: dict):
        self._body = json.dumps(payload).encode("utf-8")

    def read(self):
        return self._body

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def test_parse_success(monkeypatch):
    def fake_urlopen(req, timeout=None):
        inner = json.dumps({"action": "set", "setting": "tpm", "value": False})
        return _FakeResponse({"response": inner})

    monkeypatch.setattr(llm_parser.urllib.request, "urlopen", fake_urlopen)

    cmd = llm_parser.parse("switch off the trusted platform module thingy")
    assert cmd.action == "set"
    assert cmd.setting == "tpm"
    assert cmd.value is False
    assert cmd.source == "llm"


def test_parse_unavailable(monkeypatch):
    def fake_urlopen(req, timeout=None):
        raise urllib.error.URLError("connection refused")

    monkeypatch.setattr(llm_parser.urllib.request, "urlopen", fake_urlopen)

    with pytest.raises(llm_parser.LLMUnavailableError):
        llm_parser.parse("do something")


def test_parse_rejects_unknown_setting(monkeypatch):
    def fake_urlopen(req, timeout=None):
        inner = json.dumps({"action": "set", "setting": "made_up", "value": True})
        return _FakeResponse({"response": inner})

    monkeypatch.setattr(llm_parser.urllib.request, "urlopen", fake_urlopen)

    cmd = llm_parser.parse("something wild")
    assert cmd.action == "unknown"
