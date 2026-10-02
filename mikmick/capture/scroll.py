"""자동 스크롤 캡처: 영역을 반복 캡처하면서 스크롤하고 겹치는 부분을 찾아 이어 붙인다."""

from __future__ import annotations

import subprocess

import numpy as np
from PySide6.QtCore import QObject, QRect, QTimer, Signal
from PySide6.QtGui import QImage, QPixmap

from mikmick.capture.backend import grab_live, has_tool, is_x11


def qimage_to_array(img: QImage) -> np.ndarray:
    img = img.convertToFormat(QImage.Format.Format_ARGB32)
    w, h = img.width(), img.height()
    buf = np.frombuffer(img.constBits(), dtype=np.uint32, count=img.bytesPerLine() * h // 4)
    return buf.reshape(h, img.bytesPerLine() // 4)[:, :w].copy()


def array_to_qimage(arr: np.ndarray) -> QImage:
    arr = np.ascontiguousarray(arr, dtype=np.uint32)
    h, w = arr.shape
    img = QImage(arr.data, w, h, w * 4, QImage.Format.Format_ARGB32)
    return img.copy()


def find_overlap(prev: np.ndarray, cur: np.ndarray, min_overlap: int = 8) -> int | None:
    """prev 아래쪽과 cur 위쪽이 겹치는 높이를 찾아 cur 에서 새로 추가된 시작 행을 반환.

    반환값 k: cur[k:] 가 새 내용. 변화가 없으면 None.
    """
    h = min(prev.shape[0], cur.shape[0])
    if prev.shape != cur.shape:
        return 0
    if np.array_equal(prev, cur):
        return None
    prev_rows = [hash(r.tobytes()) for r in prev]
    cur_rows = [hash(r.tobytes()) for r in cur]
    probe_count = 12
    # cur[0:h-d] == prev[d:h] 를 만족하는 가장 작은 스크롤 거리 d 를 찾는다.
    for d in range(1, h - min_overlap + 1):
        overlap = h - d
        step = max(1, overlap // probe_count)
        if all(prev_rows[d + i] == cur_rows[i] for i in range(0, overlap, step)):
            if prev_rows[d:] == cur_rows[:overlap]:
                return overlap
    return 0


def stitch(frames: list[np.ndarray]) -> np.ndarray:
    if not frames:
        raise ValueError("no frames")
    parts = [frames[0]]
    for prev, cur in zip(frames, frames[1:], strict=False):
        k = find_overlap(prev, cur)
        if k is None:
            continue
        parts.append(cur[k:])
    return np.vstack(parts)


class ScrollCapture(QObject):
    """X11 + xdotool 환경에서 지정된 영역을 스크롤하며 캡처."""

    finished = Signal(QPixmap)
    failed = Signal(str)

    def __init__(self, rect: QRect, delay_ms: int = 100, max_frames: int = 60, parent: QObject | None = None):
        super().__init__(parent)
        self.rect = rect
        self.delay_ms = max(30, delay_ms)
        self.max_frames = max_frames
        self.frames: list[np.ndarray] = []
        self.timer = QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.timeout.connect(self._step)

    @staticmethod
    def supported() -> tuple[bool, str]:
        if not is_x11():
            return False, "자동 스크롤 캡처는 현재 X11 세션에서만 지원됩니다."
        if not has_tool("xdotool"):
            return False, "자동 스크롤 캡처에는 xdotool 이 필요합니다.\n예) sudo apt install xdotool"
        return True, ""

    def start(self) -> None:
        ok, msg = self.supported()
        if not ok:
            self.failed.emit(msg)
            return
        c = self.rect.center()
        subprocess.run(["xdotool", "mousemove", str(c.x()), str(c.y())], check=False)
        self.timer.start(self.delay_ms)

    def _grab(self) -> np.ndarray | None:
        pm = grab_live(self.rect)
        if pm is None:
            return None
        return qimage_to_array(pm.toImage())

    def _step(self) -> None:
        frame = self._grab()
        if frame is None:
            self.failed.emit("화면을 캡처할 수 없습니다.")
            return
        if self.frames and find_overlap(self.frames[-1], frame) is None:
            self._done()
            return
        self.frames.append(frame)
        if len(self.frames) >= self.max_frames:
            self._done()
            return
        subprocess.run(["xdotool", "click", "--repeat", "3", "--delay", "10", "5"], check=False)
        self.timer.start(self.delay_ms)

    def _done(self) -> None:
        if not self.frames:
            self.failed.emit("캡처된 이미지가 없습니다.")
            return
        img = array_to_qimage(stitch(self.frames))
        self.finished.emit(QPixmap.fromImage(img))
