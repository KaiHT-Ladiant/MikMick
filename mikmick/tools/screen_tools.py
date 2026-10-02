"""돋보기 / 눈금자 / 십자선 / 각도기."""

from __future__ import annotations

import math

from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, Qt, QTimer
from PySide6.QtGui import (
    QAction,
    QActionGroup,
    QColor,
    QContextMenuEvent,
    QCursor,
    QFont,
    QGuiApplication,
    QKeyEvent,
    QMouseEvent,
    QPainter,
    QPaintEvent,
    QPen,
    QPixmap,
    QWheelEvent,
)
from PySide6.QtWidgets import QInputDialog, QMenu, QWidget

from mikmick.capture.backend import Snapshot, grab_live, is_wayland
from mikmick.capture.overlay import FrozenOverlay
from mikmick.config import Config
from mikmick.icons import icon


class Magnifier(QWidget):
    """마우스 주변을 실시간으로 확대해 보여주는 창 (X11)."""

    def __init__(self, config: Config, snapshot: Snapshot | None = None):
        super().__init__(None)
        self.config = config
        self.snapshot = snapshot
        self.zoom = int(config.get("capture", "magnifier_zoom", 6)) or 4
        self.follow = True
        self.frame = QPixmap()
        self.setWindowTitle("돋보기")
        self.setWindowIcon(icon("tool_magnifier"))
        self.setWindowFlags(Qt.WindowType.Window | Qt.WindowType.WindowStaysOnTopHint)
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.resize(360, 260)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
        self.timer.start(40)
        self.setToolTip("휠: 배율 변경   스페이스: 따라가기 일시정지   ESC: 닫기")

    def _tick(self) -> None:
        if not self.follow:
            return
        pos = QCursor.pos()
        if self.frameGeometry().contains(pos):
            return
        w = max(1, self.width() // self.zoom)
        h = max(1, self.height() // self.zoom)
        rect = QRect(pos.x() - w // 2, pos.y() - h // 2, w, h)
        pm = None
        if self.snapshot is not None:
            pm = self.snapshot.crop(rect)
        else:
            pm = grab_live(rect)
        if pm is not None and not pm.isNull():
            self.frame = pm
            self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        p.fillRect(self.rect(), QColor("#202020"))
        if not self.frame.isNull():
            p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, False)
            p.drawPixmap(self.rect(), self.frame)
        c = self.rect().center()
        p.setPen(QPen(QColor(255, 60, 60, 200), 1))
        p.drawLine(c.x() - 10, c.y(), c.x() + 10, c.y())
        p.drawLine(c.x(), c.y() - 10, c.x(), c.y() + 10)
        p.setPen(QColor("white"))
        p.drawText(self.rect().adjusted(6, 4, -6, -4), Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignRight, f"{self.zoom}x")
        p.end()

    def wheelEvent(self, event: QWheelEvent) -> None:
        self.zoom = max(2, min(32, self.zoom + (1 if event.angleDelta().y() > 0 else -1)))
        self.update()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Escape:
            self.close()
        elif event.key() == Qt.Key.Key_Space:
            self.follow = not self.follow
        else:
            super().keyPressEvent(event)


class Ruler(QWidget):
    """화면 위에 띄우는 반투명 눈금자."""

    UNITS = {"px": "픽셀", "in": "인치", "cm": "센티미터"}

    def __init__(self, config: Config):
        super().__init__(None)
        self.config = config
        self.horizontal = True
        self.unit = config.get("ruler", "unit", "px")
        self.dpi = int(config.get("ruler", "dpi", 96))
        self._drag: QPoint | None = None
        self._resize = False
        self.setWindowTitle("눈금자")
        self.setWindowIcon(icon("tool_ruler"))
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.WindowStaysOnTopHint | Qt.WindowType.Tool)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.setMouseTracking(True)
        self.resize(800, 60)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update)
        self.timer.start(50)
        self.setToolTip("드래그: 이동   끝 부분 드래그: 길이 조절   더블클릭: 방향 전환   우클릭: 메뉴")

    def _pixels_per_unit(self) -> float:
        return {"px": 1.0, "in": float(self.dpi), "cm": self.dpi / 2.54}[self.unit]

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        p.fillRect(self.rect(), QColor(255, 243, 200, 225))
        p.setPen(QColor("#a07020"))
        p.drawRect(self.rect().adjusted(0, 0, -1, -1))
        length = self.width() if self.horizontal else self.height()
        ppu = self._pixels_per_unit()
        if self.unit == "px":
            minor, mid, major = 2, 10, 50
            step_px = 1.0
        elif self.unit == "in":
            minor, mid, major = 1, 4, 16
            step_px = ppu / 16
        else:
            minor, mid, major = 1, 5, 10
            step_px = ppu / 10
        f = QFont()
        f.setPixelSize(10)
        p.setFont(f)
        i = 0
        while True:
            pos = i * step_px
            if pos > length:
                break
            if i % minor == 0:
                if i % major == 0:
                    size = 18
                elif i % mid == 0:
                    size = 11
                else:
                    size = 5
                x = int(pos)
                if self.horizontal:
                    p.drawLine(x, 0, x, size)
                    if i % major == 0 and i:
                        label = str(i) if self.unit == "px" else str(i // major)
                        p.drawText(x + 2, 30, label)
                else:
                    p.drawLine(0, x, size, x)
                    if i % major == 0 and i:
                        label = str(i) if self.unit == "px" else str(i // major)
                        p.drawText(22, x + 4, label)
            i += 1
        cur = self.mapFromGlobal(QCursor.pos())
        p.setPen(QPen(QColor("#d9452b"), 1))
        val = (cur.x() if self.horizontal else cur.y()) / ppu
        text = f"{val:.0f} px" if self.unit == "px" else f"{val:.2f} {self.unit}"
        if self.horizontal and 0 <= cur.x() <= self.width():
            p.drawLine(cur.x(), 0, cur.x(), self.height())
            p.drawText(QRect(0, 0, self.width() - 6, self.height() - 4), Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignBottom, text)
        elif not self.horizontal and 0 <= cur.y() <= self.height():
            p.drawLine(0, cur.y(), self.width(), cur.y())
            p.drawText(QRect(0, 0, self.width() - 4, self.height() - 6), Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignBottom, text)
        p.end()

    def _near_end(self, pos: QPoint) -> bool:
        return (self.width() - pos.x() < 10) if self.horizontal else (self.height() - pos.y() < 10)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            pos = event.position().toPoint()
            self._resize = self._near_end(pos)
            self._drag = event.globalPosition().toPoint() - self.frameGeometry().topLeft()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        pos = event.position().toPoint()
        if self._drag is None:
            cursor = (
                (Qt.CursorShape.SizeHorCursor if self.horizontal else Qt.CursorShape.SizeVerCursor)
                if self._near_end(pos)
                else Qt.CursorShape.SizeAllCursor
            )
            self.setCursor(cursor)
            return
        if self._resize:
            if self.horizontal:
                self.resize(max(100, pos.x()), self.height())
            else:
                self.resize(self.width(), max(100, pos.y()))
        else:
            self.move(event.globalPosition().toPoint() - self._drag)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        self._drag = None
        self._resize = False

    def mouseDoubleClickEvent(self, event: QMouseEvent) -> None:
        self.toggle_orientation()

    def toggle_orientation(self) -> None:
        self.horizontal = not self.horizontal
        self.resize(self.height(), self.width())

    def contextMenuEvent(self, event: QContextMenuEvent) -> None:
        menu = QMenu(self)
        menu.addAction("가로/세로 전환", self.toggle_orientation)
        unit_menu = menu.addMenu("단위")
        group = QActionGroup(menu)
        for key, label in self.UNITS.items():
            a = QAction(label, menu)
            a.setCheckable(True)
            a.setChecked(key == self.unit)
            a.triggered.connect(lambda _=False, k=key: self._set_unit(k))
            group.addAction(a)
            unit_menu.addAction(a)
        menu.addAction(f"DPI 설정... ({self.dpi})", self._set_dpi)
        menu.addSeparator()
        menu.addAction("닫기", self.close)
        menu.exec(event.globalPos())

    def _set_unit(self, unit: str) -> None:
        self.unit = unit
        self.config.set("ruler", "unit", unit)
        self.config.save()

    def _set_dpi(self) -> None:
        val, ok = QInputDialog.getInt(self, "DPI", "DPI:", self.dpi, 30, 1200)
        if ok:
            self.dpi = val
            self.config.set("ruler", "dpi", val)
            self.config.save()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Escape:
            self.close()
            return
        step = 10 if event.modifiers() & Qt.KeyboardModifier.ShiftModifier else 1
        d = {Qt.Key.Key_Left: (-step, 0), Qt.Key.Key_Right: (step, 0), Qt.Key.Key_Up: (0, -step), Qt.Key.Key_Down: (0, step)}
        if event.key() in d:
            dx, dy = d[event.key()]
            self.move(self.x() + dx, self.y() + dy)
            return
        super().keyPressEvent(event)

    def start(self) -> None:
        screen = QGuiApplication.screenAt(QCursor.pos()) or QGuiApplication.primaryScreen()
        if screen is not None:
            g = screen.availableGeometry()
            self.move(g.center().x() - self.width() // 2, g.center().y() - self.height() // 2)
        self.show()
        self.raise_()
        self.activateWindow()


class CrosshairOverlay(FrozenOverlay):
    """십자선: 절대 좌표 / 클릭 지점 기준 상대 좌표."""

    def __init__(self, snapshot: Snapshot, zoom: int = 6):
        super().__init__(snapshot, True, zoom)
        self.origin: QPoint | None = None
        self.marks: list[QPoint] = []

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        self.mouse = event.position().toPoint()
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            self.cancel()
            return
        if event.button() == Qt.MouseButton.LeftButton:
            p = event.position().toPoint()
            if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
                self.marks.append(p)
            else:
                self.origin = p
            self.update()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_C and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            gp = self.to_global(self.mouse)
            QGuiApplication.clipboard().setText(f"{int(gp.x())}, {int(gp.y())}")
            return
        if event.key() == Qt.Key.Key_R:
            self.origin = None
            self.marks.clear()
            self.update()
            return
        super().keyPressEvent(event)

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        self.paint_background(p)
        p.fillRect(self.rect(), QColor(0, 0, 0, 40))
        pen = QPen(QColor(255, 40, 40), 1)
        p.setPen(pen)
        p.drawLine(0, self.mouse.y(), self.width(), self.mouse.y())
        p.drawLine(self.mouse.x(), 0, self.mouse.x(), self.height())
        extra = []
        if self.origin is not None:
            p.setPen(QPen(QColor(47, 140, 255), 1, Qt.PenStyle.DashLine))
            p.drawLine(0, self.origin.y(), self.width(), self.origin.y())
            p.drawLine(self.origin.x(), 0, self.origin.x(), self.height())
            p.drawRect(QRect(self.origin, self.mouse).normalized())
            go = self.to_global(self.origin)
            gm = self.to_global(self.mouse)
            dx, dy = int(gm.x() - go.x()), int(gm.y() - go.y())
            extra.append(f"상대: {dx}, {dy}  (거리 {math.hypot(dx, dy):.1f})")
        for m in self.marks:
            p.setPen(QPen(QColor(255, 200, 0), 2))
            p.drawEllipse(m, 4, 4)
        self.draw_magnifier(p, extra)
        self.draw_hint(p, "클릭: 기준점 지정   Ctrl+클릭: 표시   R: 초기화   Ctrl+C: 좌표 복사   ESC: 닫기")
        p.end()


class ProtractorOverlay(FrozenOverlay):
    """각도기: 꼭짓점 → 첫 번째 선 → 두 번째 선 순서로 클릭."""

    def __init__(self, snapshot: Snapshot, zoom: int = 6):
        super().__init__(snapshot, False, zoom)
        self.points: list[QPointF] = []

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        self.mouse = event.position().toPoint()
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            if self.points:
                self.points.pop()
                self.update()
            else:
                self.cancel()
            return
        if event.button() == Qt.MouseButton.LeftButton:
            if len(self.points) >= 3:
                self.points = []
            self.points.append(QPointF(event.position()))
            self.update()

    @staticmethod
    def _angle(v: QPointF, a: QPointF) -> float:
        return math.degrees(math.atan2(-(a.y() - v.y()), a.x() - v.x()))

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        self.paint_background(p)
        p.fillRect(self.rect(), QColor(0, 0, 0, 40))
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        pts = list(self.points)
        if len(pts) < 3:
            pts.append(QPointF(self.mouse))
        pen = QPen(QColor(47, 140, 255), 2)
        p.setPen(pen)
        f = QFont()
        f.setPixelSize(14)
        f.setBold(True)
        p.setFont(f)
        if len(pts) >= 2:
            v = pts[0]
            p.drawLine(v, pts[1])
            a1 = self._angle(v, pts[1])
            if len(pts) == 2:
                p.setPen(QPen(QColor(255, 255, 255, 140), 1, Qt.PenStyle.DashLine))
                p.drawLine(v, QPointF(v.x() + 200, v.y()))
                self._label(p, v, f"{a1 % 360:.1f}°")
            if len(pts) >= 3:
                p.setPen(pen)
                p.drawLine(v, pts[2])
                a2 = self._angle(v, pts[2])
                diff = (a2 - a1) % 360
                inner = diff if diff <= 180 else 360 - diff
                r = 40
                p.setPen(QPen(QColor(255, 200, 0), 2))
                start = a1 if diff <= 180 else a2
                p.drawArc(QRectF(v.x() - r, v.y() - r, 2 * r, 2 * r), int(start * 16), int(inner * 16))
                self._label(p, v, f"{inner:.1f}°  (반대 {360 - inner:.1f}°)")
        for pt in self.points:
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor(255, 60, 60))
            p.drawEllipse(pt, 4, 4)
        self.draw_hint(p, "1) 꼭짓점 클릭  2) 첫 번째 선 클릭  3) 두 번째 선 클릭   우클릭: 되돌리기   ESC: 닫기")
        p.end()

    def _label(self, p: QPainter, v: QPointF, text: str) -> None:
        r = QRectF(v.x() + 14, v.y() + 10, p.fontMetrics().horizontalAdvance(text) + 16, 26)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(20, 20, 20, 210))
        p.drawRoundedRect(r, 5, 5)
        p.setPen(QColor("white"))
        p.drawText(r, Qt.AlignmentFlag.AlignCenter, text)


def needs_snapshot_for_live_tools() -> bool:
    return is_wayland()
