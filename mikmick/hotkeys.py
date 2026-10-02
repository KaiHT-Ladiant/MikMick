"""전역 단축키 (X11: pynput). Wayland 에서는 데스크톱 환경의 단축키 설정을 이용한다."""

from __future__ import annotations

from PySide6.QtCore import QObject, Signal

from mikmick.capture.backend import is_wayland

_MODS = {"Shift": "<shift>", "Ctrl": "<ctrl>", "Alt": "<alt>"}


def to_pynput(seq: str) -> str | None:
    if not seq:
        return None
    parts = []
    for token in seq.split("+"):
        if token in _MODS:
            parts.append(_MODS[token])
        elif token == "Print":
            parts.append("<print_screen>")
        elif token.startswith("F") and token[1:].isdigit():
            parts.append(f"<{token.lower()}>")
        elif len(token) == 1:
            parts.append(token.lower())
        else:
            return None
    return "+".join(parts)


class HotkeyManager(QObject):
    triggered = Signal(str)

    def __init__(self):
        super().__init__()
        self._listener = None
        self.error = ""

    @staticmethod
    def available() -> tuple[bool, str]:
        if is_wayland():
            return False, "Wayland 세션에서는 전역 단축키를 데스크톱 설정에서 등록해야 합니다."
        try:
            import pynput  # noqa: F401
        except ImportError:
            return False, "전역 단축키를 사용하려면 pynput 패키지가 필요합니다. (pip install pynput)"
        return True, ""

    def apply(self, hotkeys: dict[str, str]) -> None:
        self.stop()
        ok, msg = self.available()
        if not ok:
            self.error = msg
            return
        from pynput import keyboard

        mapping = {}
        for action, seq in hotkeys.items():
            combo = to_pynput(seq)
            if combo:
                mapping[combo] = lambda a=action: self.triggered.emit(a)
        if not mapping:
            return
        try:
            self._listener = keyboard.GlobalHotKeys(mapping)
            self._listener.daemon = True
            self._listener.start()
            self.error = ""
        except Exception as exc:  # pragma: no cover - 환경 의존
            self._listener = None
            self.error = str(exc)

    def stop(self) -> None:
        if self._listener is not None:
            try:
                self._listener.stop()
            except Exception:
                pass
            self._listener = None
