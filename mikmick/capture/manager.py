"""캡처 모드별 흐름 제어."""

from __future__ import annotations

import os
import shutil
import subprocess
from collections.abc import Callable

from PySide6.QtCore import QObject, QPoint, QPointF, QRect, QSize, Qt, QTimer, Signal
from PySide6.QtGui import QColor, QCursor, QPainter, QPainterPath, QPen, QPixmap, QPolygonF
from PySide6.QtWidgets import QApplication, QDialog, QDialogButtonBox, QFormLayout, QSpinBox

from mikmick.capture import backend
from mikmick.capture.overlay import CaptureOverlay
from mikmick.capture.scroll import ScrollCapture
from mikmick.config import Config

MODE_LABELS = {
    "fullscreen": "전체화면 캡처하기",
    "active_window": "활성화된 윈도우 캡처",
    "window_control": "윈도우 컨트롤 캡처",
    "scroll": "자동 스크롤 캡처",
    "region": "영역을 지정하여 캡처",
    "fixed": "고정된 사각 영역 캡처",
    "freehand": "내 마음대로 캡처하기",
    "repeat_last": "마지막 캡처 영역 반복",
}


def draw_cursor(pixmap: QPixmap, pos: QPoint) -> None:
    """캡처 이미지에 기본 화살표 커서를 그린다 (pos: 이미지 픽셀 좌표)."""
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.translate(pos)
    poly = QPolygonF(
        [QPointF(0, 0), QPointF(0, 17), QPointF(4, 13), QPointF(7, 20), QPointF(10, 19), QPointF(7, 12), QPointF(12, 12)]
    )
    painter.setPen(QPen(QColor("black"), 1.2))
    painter.setBrush(QColor("white"))
    painter.drawPolygon(poly)
    painter.end()


def play_shutter() -> None:
    for cmd in (
        ["canberra-gtk-play", "-i", "screen-capture"],
        ["paplay", "/usr/share/sounds/freedesktop/stereo/screen-capture.oga"],
    ):
        if shutil.which(cmd[0]) and (len(cmd) < 3 or cmd[0] != "paplay" or os.path.exists(cmd[1])):
            try:
                subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return
            except OSError:
                continue
    QApplication.beep()


class FixedSizeDialog(QDialog):
    def __init__(self, size: QSize, parent=None):
        super().__init__(parent)
        self.setWindowTitle("고정된 사각 영역 크기")
        form = QFormLayout(self)
        self.w = QSpinBox()
        self.w.setRange(10, 10000)
        self.w.setValue(size.width())
        self.h = QSpinBox()
        self.h.setRange(10, 10000)
        self.h.setValue(size.height())
        form.addRow("가로 (Width)", self.w)
        form.addRow("세로 (Height)", self.h)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self) -> QSize:
        return QSize(self.w.value(), self.h.value())


class CaptureManager(QObject):
    captured = Signal(QPixmap, str)  # 이미지, 모드
    failed = Signal(str)

    def __init__(self, config: Config, hide_windows: Callable[[], bool], restore_windows: Callable[[], None]):
        super().__init__()
        self.config = config
        self.hide_windows = hide_windows
        self.restore_windows = restore_windows
        self.busy = False
        self._overlay: CaptureOverlay | None = None
        self._scroll: ScrollCapture | None = None

    # 진입점 ---------------------------------------------------------------
    def capture(self, mode: str, delay_ms: int | None = None) -> None:
        if self.busy:
            return
        if mode == "fixed":
            size = QSize(int(self.config.get("capture", "fixed_width", 640)), int(self.config.get("capture", "fixed_height", 480)))
            dlg = FixedSizeDialog(size)
            if dlg.exec() != QDialog.DialogCode.Accepted:
                return
            self.config.set("capture", "fixed_width", dlg.value().width())
            self.config.set("capture", "fixed_height", dlg.value().height())
            self.config.save()
        self.busy = True
        hidden = self.hide_windows()
        delay = int(self.config.get("capture", "delay_ms", 0)) if delay_ms is None else delay_ms
        wait = max(delay, 350 if hidden else 80)
        QTimer.singleShot(wait, lambda: self._run(mode))

    # 내부 -----------------------------------------------------------------
    def _snapshot(self) -> backend.Snapshot | None:
        snap = backend.grab_screen(bool(self.config.get("capture", "multi_monitor", True)))
        if snap is None:
            self._fail("화면을 캡처할 수 없습니다.\nWayland 환경이라면 xdg-desktop-portal 과 jeepney 또는 grim 을 설치하세요.")
        return snap

    def _run(self, mode: str) -> None:
        self._cursor = QCursor.pos()
        if mode == "repeat_last":
            last = self.config.get("recent", "last_region")
            if last:
                snap = self._snapshot()
                if snap is None:
                    return
                rect = QRect(*last)
                self._deliver(snap.crop(rect), rect, "repeat_last")
                return
            mode = "region"

        if mode == "active_window":
            info = backend.active_window_rect()
            if info is not None and info[1] != os.getpid():
                snap = self._snapshot()
                if snap is None:
                    return
                rect = info[0].intersected(snap.geometry)
                self._deliver(snap.crop(rect), rect, mode)
                return
            mode_overlay = "window"
        elif mode == "window_control":
            mode_overlay = "window"
        elif mode == "fullscreen":
            snap = self._snapshot()
            if snap is None:
                return
            self._deliver(snap.crop(snap.geometry), snap.geometry, mode)
            return
        elif mode == "scroll":
            mode_overlay = "window"
        else:
            mode_overlay = mode

        snap = self._snapshot()
        if snap is None:
            return
        windows = backend.window_rects() if mode_overlay == "window" else []
        overlay = CaptureOverlay(
            snap,
            mode=mode_overlay,
            magnifier=bool(self.config.get("capture", "magnifier", True)),
            zoom=int(self.config.get("capture", "magnifier_zoom", 6)),
            fixed_size=QSize(int(self.config.get("capture", "fixed_width", 640)), int(self.config.get("capture", "fixed_height", 480))),
            windows=windows,
            show_hint=bool(self.config.get("capture", "show_toolbar", True)),
        )
        overlay.selected.connect(lambda rect, path: self._on_selected(snap, rect, path, mode))
        overlay.cancelled.connect(self._cancelled)
        self._overlay = overlay
        overlay.start()

    def _on_selected(self, snap: backend.Snapshot, rect: QRect, path: QPainterPath | None, mode: str) -> None:
        self._overlay = None
        if mode == "scroll":
            ok, msg = ScrollCapture.supported()
            if not ok:
                self._deliver(snap.crop(rect), rect, mode)
                self.failed.emit(msg + "\n선택한 영역만 캡처했습니다.")
                return
            self._scroll = ScrollCapture(rect, int(self.config.get("capture", "scroll_delay_ms", 100)), parent=self)
            self._scroll.finished.connect(lambda pm: self._deliver(pm, rect, mode, remember=False))
            self._scroll.failed.connect(self._fail)
            QTimer.singleShot(150, self._scroll.start)
            return
        pm = snap.crop(rect)
        if path is not None:
            masked = QPixmap(pm.size())
            masked.fill(Qt.GlobalColor.transparent)
            painter = QPainter(masked)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setClipPath(path)
            painter.drawPixmap(0, 0, pm)
            painter.end()
            pm = masked
        self._deliver(pm, rect, mode, remember=path is None)

    def _deliver(self, pm: QPixmap, rect: QRect, mode: str, remember: bool = True) -> None:
        if remember and mode != "fullscreen":
            self.config.set("recent", "last_region", [rect.x(), rect.y(), rect.width(), rect.height()])
            self.config.save()
        if self.config.get("capture", "include_cursor", False) and mode != "scroll":
            local = self._cursor - rect.topLeft()
            if QRect(QPoint(0, 0), rect.size()).contains(local):
                sx = pm.width() / max(1, rect.width())
                sy = pm.height() / max(1, rect.height())
                draw_cursor(pm, QPoint(round(local.x() * sx), round(local.y() * sy)))
        if self.config.get("capture", "sound", True):
            play_shutter()
        self.busy = False
        self.restore_windows()
        self.captured.emit(pm, mode)

    def _cancelled(self) -> None:
        self._overlay = None
        self.busy = False
        self.restore_windows()

    def _fail(self, message: str) -> None:
        self.busy = False
        self.restore_windows()
        self.failed.emit(message)
