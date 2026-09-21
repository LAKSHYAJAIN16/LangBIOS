import json

import pytest

from langbios.bios_state import BiosState, InvalidValueError, UnknownSettingError


@pytest.fixture
def state(tmp_path):
    return BiosState.load(tmp_path / "state.json")


def test_defaults_when_no_file(state):
    assert state.get("secure_boot") is True
    assert state.get("boot_order") == ["ssd", "usb", "network", "hdd", "cdrom"]


def test_set_bool(state):
    state.set("virtualization", "on")
    assert state.get("virtualization") is True
    state.set("virtualization", "off")
    assert state.get("virtualization") is False


def test_set_enum(state):
    state.set("fan_profile", "Full Speed")
    assert state.get("fan_profile") == "full_speed"


def test_set_enum_invalid(state):
    with pytest.raises(InvalidValueError):
        state.set("fan_profile", "ludicrous")


def test_unknown_setting(state):
    with pytest.raises(UnknownSettingError):
        state.get("does_not_exist")


def test_persists_to_disk(tmp_path):
    path = tmp_path / "state.json"
    s1 = BiosState.load(path)
    s1.set("tpm", "off")

    s2 = BiosState.load(path)
    assert s2.get("tpm") is False
    assert json.loads(path.read_text())["tpm"] is False


def test_reset(state):
    state.set("tpm", "off")
    state.reset()
    assert state.get("tpm") is True
