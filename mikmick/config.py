"""사용자 설정 저장소 ($XDG_CONFIG_HOME/mikmick/config.json)."""

from __future__ import annotations

import copy
import json
import os
from pathlib import Path
from typing import Any

CAPTURE_MODES = [
    "fullscreen",
    "active_window",
    "window_control",
    "scroll",
    "region",
    "fixed",
    "freehand",
    "repeat_last",
]

TOOL_NAMES = [
    "editor",
    "color_picker",
    "palette",
    "magnifier",
    "ruler",
    "protractor",
    "crosshair",
    "whiteboard",
]

DEFAULT_HOTKEYS = {
    "fullscreen": "Print",
    "active_window": "Alt+Print",
    "window_control": "Ctrl+Print",
    "scroll": "Ctrl+Alt+Print",
    "region": "Shift+Print",
    "fixed": "Shift+Ctrl+Print",
    "freehand": "Shift+Ctrl+Alt+Print",
    "repeat_last": "",
    "editor": "",
    "color_picker": "",
    "palette": "",
    "magnifier": "",
    "ruler": "",
    "protractor": "",
    "crosshair": "",
    "whiteboard": "",
}

DEFAULTS: dict[str, dict[str, Any]] = {
    "general": {
        "start_mode": "editor",  # editor | tray
        "autostart": False,
        "autostart_mode": "tray",  # tray | editor
        "check_updates": True,
        "hide_start_page": False,
    },
    "editor": {
        "hide_on_capture": True,
        "center_image": False,
        "auto_shape_selection": False,
        "no_save_prompt": False,
        "quit_on_close": False,
        "bg_mode": "solid",  # solid | grid
        "bg_color": "#dcdcdc",
    },
    "capture": {
        "result": "editor",  # editor | clipboard | file | autosave | ftp | program
        "multi_monitor": True,
        "sound": True,
        "include_cursor": False,
        "always_clipboard": False,
        "show_toolbar": True,
        "delay_ms": 0,
        "scroll_delay_ms": 100,
        "magnifier": True,
        "magnifier_zoom": 6,
        "fixed_width": 640,
        "fixed_height": 480,
    },
    "filename": {
        "pattern": "%c",
        "format": "png",
        "counter": 0,
    },
    "autosave": {
        "enabled": False,
        "folder": "",
        "program": "",
        "program_args": "",
    },
    "image": {
        "jpeg_quality": 100,
    },
    "ftp": {
        "server": "",
        "port": 21,
        "path": "",
        "passive": False,
        "user": "anonymous",
        "password": "",
        "open_url": False,
        "copy_url": False,
        "url": "",
    },
    "hotkeys": dict(DEFAULT_HOTKEYS),
    "new_image": {
        "preset": "clipboard",
        "width": 1131,
        "height": 720,
        "bg": "#ffffff",
    },
    "palette": {
        "custom": [],
        "format": "hex",
    },
    "ruler": {
        "unit": "px",
        "dpi": 96,
    },
    "recent": {
        "files": [],
        "last_region": None,
        "last_mode": None,
    },
}


def config_dir() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    return Path(base) / "mikmick"


def pictures_dir() -> Path:
    base = os.environ.get("XDG_PICTURES_DIR")
    if base:
        return Path(base)
    pics = Path(os.path.expanduser("~")) / "Pictures"
    return pics if pics.is_dir() else Path(os.path.expanduser("~"))


class Config:
    def __init__(self, path: Path | None = None):
        self.path = path or (config_dir() / "config.json")
        self.data: dict[str, dict[str, Any]] = copy.deepcopy(DEFAULTS)
        self.load()

    def load(self) -> None:
        try:
            with open(self.path, encoding="utf-8") as fp:
                stored = json.load(fp)
        except (OSError, ValueError):
            return
        if not isinstance(stored, dict):
            return
        for section, values in stored.items():
            if section in self.data and isinstance(values, dict):
                self.data[section].update(values)

    def save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_suffix(".tmp")
        with open(tmp, "w", encoding="utf-8") as fp:
            json.dump(self.data, fp, ensure_ascii=False, indent=2)
        os.replace(tmp, self.path)

    def get(self, section: str, key: str, default: Any = None) -> Any:
        return self.data.get(section, {}).get(key, default)

    def set(self, section: str, key: str, value: Any) -> None:
        self.data.setdefault(section, {})[key] = value

    def section(self, section: str) -> dict[str, Any]:
        return self.data.setdefault(section, {})

    def snapshot(self) -> dict[str, dict[str, Any]]:
        return copy.deepcopy(self.data)

    def restore(self, data: dict[str, dict[str, Any]]) -> None:
        self.data = copy.deepcopy(data)

    def add_recent_file(self, path: str, limit: int = 10) -> None:
        files = [p for p in self.get("recent", "files", []) if p != path]
        files.insert(0, path)
        self.set("recent", "files", files[:limit])
