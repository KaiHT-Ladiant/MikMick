"""데스크톱 연동: 자동 실행 등록, 업데이트 확인."""

from __future__ import annotations

import json
import os
import shutil
import sys
import urllib.request
from pathlib import Path

from mikmick import APP_NAME, RELEASES_API, __version__

AUTOSTART_NAME = "mikmick.desktop"


def autostart_path() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    return Path(base) / "autostart" / AUTOSTART_NAME


def launcher_command() -> str:
    exe = shutil.which("mikmick")
    if exe:
        return exe
    return f"{sys.executable} -m mikmick"


def set_autostart(enabled: bool, tray_only: bool = True) -> None:
    path = autostart_path()
    if not enabled:
        try:
            path.unlink()
        except FileNotFoundError:
            pass
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    args = " --tray" if tray_only else ""
    path.write_text(
        "[Desktop Entry]\n"
        "Type=Application\n"
        f"Name={APP_NAME}\n"
        "Name[ko]=믹믹\n"
        "Comment=Screen capture and image editor\n"
        f"Exec={launcher_command()}{args}\n"
        "Icon=mikmick\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n",
        encoding="utf-8",
    )


def parse_version(v: str) -> tuple[int, ...]:
    v = v.strip().lstrip("vV")
    out = []
    for part in v.split("."):
        num = ""
        for ch in part:
            if ch.isdigit():
                num += ch
            else:
                break
        out.append(int(num) if num else 0)
    return tuple(out)


def fetch_latest_release(timeout: float = 8.0) -> tuple[str, str] | None:
    """(버전, URL) 반환. 실패 시 None."""
    req = urllib.request.Request(RELEASES_API, headers={"Accept": "application/vnd.github+json", "User-Agent": f"MikMick/{__version__}"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except Exception:
        return None
    tag = data.get("tag_name") or ""
    if not tag:
        return None
    return tag, data.get("html_url") or ""


def is_newer(remote: str, local: str = __version__) -> bool:
    return parse_version(remote) > parse_version(local)
