import json

from mikmick.config import DEFAULT_HOTKEYS, Config


def test_defaults(config):
    assert config.get("capture", "result") == "editor"
    assert config.get("hotkeys", "region") == DEFAULT_HOTKEYS["region"]
    assert config.get("filename", "pattern") == "%c"


def test_roundtrip(config):
    config.set("capture", "delay_ms", 1500)
    config.add_recent_file("/tmp/a.png")
    config.save()
    again = Config(config.path)
    assert again.get("capture", "delay_ms") == 1500
    assert again.get("recent", "files") == ["/tmp/a.png"]


def test_unknown_sections_are_ignored_and_missing_keys_default(config):
    config.path.parent.mkdir(parents=True, exist_ok=True)
    config.path.write_text(json.dumps({"bogus": {"x": 1}, "capture": {"sound": False}}), encoding="utf-8")
    loaded = Config(config.path)
    assert "bogus" not in loaded.data
    assert loaded.get("capture", "sound") is False
    assert loaded.get("capture", "magnifier") is True


def test_corrupt_file_uses_defaults(config):
    config.path.parent.mkdir(parents=True, exist_ok=True)
    config.path.write_text("{not json", encoding="utf-8")
    assert Config(config.path).get("general", "start_mode") == "editor"
