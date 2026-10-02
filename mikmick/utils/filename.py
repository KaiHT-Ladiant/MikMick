"""자동 저장 / FTP 전송 파일 이름 패턴 처리."""

from __future__ import annotations

import getpass
import os
import re
import socket
import time
from pathlib import Path

IMAGE_FORMATS = [
    ("png", "Portable Network Graphics (*.png)"),
    ("jpg", "JPEG (*.jpg)"),
    ("bmp", "Windows Bitmap (*.bmp)"),
    ("gif", "Graphics Interchange Format (*.gif)"),
    ("webp", "WebP (*.webp)"),
    ("tiff", "Tagged Image File Format (*.tiff)"),
    ("pdf", "Portable Document Format (*.pdf)"),
]

_INVALID = re.compile(r'[/\x00]')


def expand_pattern(pattern: str, counter: int, now: float | None = None) -> str:
    """%y %m %d %h %n %s %c %u %w %t 를 실제 값으로 치환한다."""
    t = time.localtime(now if now is not None else time.time())
    try:
        user = getpass.getuser()
    except Exception:
        user = "user"
    values = {
        "y": f"{t.tm_year:04d}",
        "m": f"{t.tm_mon:02d}",
        "d": f"{t.tm_mday:02d}",
        "h": f"{t.tm_hour:02d}",
        "n": f"{t.tm_min:02d}",
        "s": f"{t.tm_sec:02d}",
        "c": f"{counter:03d}",
        "u": user,
        "w": socket.gethostname(),
        "t": str(int(now if now is not None else time.time())),
        "%": "%",
    }

    def repl(match: re.Match[str]) -> str:
        return values.get(match.group(1), match.group(0))

    name = re.sub(r"%(.)", repl, pattern or "%c")
    name = _INVALID.sub("_", name).strip()
    return name or f"{counter:03d}"


def unique_path(folder: Path, stem: str, ext: str) -> Path:
    candidate = folder / f"{stem}.{ext}"
    index = 1
    while candidate.exists():
        candidate = folder / f"{stem} ({index}).{ext}"
        index += 1
    return candidate


def ext_of(path: str) -> str:
    return os.path.splitext(path)[1].lower().lstrip(".")
