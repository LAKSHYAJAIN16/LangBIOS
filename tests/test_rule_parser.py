from langbios.rule_parser import parse


def test_enable_secure_boot():
    cmd = parse("enable secure boot")
    assert cmd.action == "set"
    assert cmd.setting == "secure_boot"
    assert cmd.value is True


def test_disable_virtualization():
    cmd = parse("turn off virtualization")
    assert cmd.action == "set"
    assert cmd.setting == "virtualization"
    assert cmd.value is False


def test_get_fan_profile():
    cmd = parse("what's my fan profile")
    assert cmd.action == "get"
    assert cmd.setting == "fan_profile"


def test_set_enum_value():
    cmd = parse("set fan profile to silent")
    assert cmd.action == "set"
    assert cmd.setting == "fan_profile"
    assert cmd.value == "silent"


def test_list_settings():
    cmd = parse("list settings")
    assert cmd.action == "list"


def test_reset_defaults():
    cmd = parse("reset to factory defaults")
    assert cmd.action == "reset"


def test_unmatched_returns_none():
    assert parse("tell me a joke") is None


def test_ambiguous_bool_without_verb_returns_none():
    assert parse("secure boot") is None
