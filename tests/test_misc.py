from PySide6.QtGui import QColor

from mikmick.hotkeys import to_pynput
from mikmick.options import build_hotkey, parse_hotkey
from mikmick.system import is_newer, parse_version
from mikmick.tools.colors import format_color


def test_hotkey_parse_build_roundtrip():
    assert parse_hotkey("Shift+Ctrl+Print") == (True, True, False, "Print")
    assert build_hotkey(True, True, False, "Print") == "Shift+Ctrl+Print"
    assert build_hotkey(True, False, False, "") == ""


def test_to_pynput():
    assert to_pynput("Alt+Print") == "<alt>+<print_screen>"
    assert to_pynput("Ctrl+F5") == "<ctrl>+<f5>"
    assert to_pynput("Shift+A") == "<shift>+a"
    assert to_pynput("") is None


def test_version_compare():
    assert parse_version("v1.2.10") == (1, 2, 10)
    assert is_newer("v0.2.0", "0.1.9")
    assert not is_newer("0.1.0", "0.1.0")


def test_color_formats():
    c = QColor(255, 136, 0)
    assert format_color(c, "html") == "#FF8800"
    assert format_color(c, "hex") == "FF8800"
    assert format_color(c, "rgb") == "rgb(255, 136, 0)"
    assert format_color(c, "cpp") == "0x000088FF"
    assert format_color(c, "delphi") == "$000088FF"
