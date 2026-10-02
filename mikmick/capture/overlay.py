"""정지된 화면 위에서 영역을 선택하는 전체 화면 오버레이."""

from __future__ import annotations

from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, QSize, Qt, QTimer, Signal
from PySide6.QtGui import (
    QColor,
    QFont,
    QGuiApplication,
    QKeyEvent,
    QMouseEvent,
    QPainter,
    QPainterPath,
    QPaintEvent,
    QPen,
    QWheelEvent,
)
from PySide6.QtWidgets import QWidget

from mikmick.capture.backend import Snapshot, is_wayland

DIM = QColor(0, 0, 0, 110)
ACCENT = QColor("#2f8cff")


class FrozenOverlay(QWidget):
    """정지 화면 스냅샷을 전체 화면으로 띄우는 공통 베이스."""

    cancelled = Signal()

    def __init__(self, snapshot: Snapshot, magnifier: bool = True, zoom: int = 6):
        super().__init__(None)
        self.snapshot = snapshot
        self.image = snapshot.pixmap.toImage()
        self.show_magnifier = magnifier
        self.zoom = max(2, min(20, zoom))
        self.mouse = QPoint(-1000, -1000)
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
            | Qt.WindowType.BypassWindowManagerHint
        )
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.setMouseTracking(True)
        self.setCursor(Qt.CursorShape.CrossCursor)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    # 좌표 변환 ------------------------------------------------------------
    def to_global(self, p: QPointF | QPoint) -> QPointF:
        g = self.snapshot.geometry
        sx = g.width() / max(1, self.width())
        sy = g.height() / max(1, self.height())
        return QPointF(g.x() + p.x() * sx, g.y() + p.y() * sy)

    def rect_to_global(self, r: QRect) -> QRect:
        tl = self.to_global(r.topLeft())
        br = self.to_global(QPointF(r.x() + r.width(), r.y() + r.height()))
        return QRect(QPoint(round(tl.x()), round(tl.y())), QPoint(round(br.x()) - 1, round(br.y()) - 1))

    def from_global_rect(self, r: QRect) -> QRect:
        g = self.snapshot.geometry
        sx = self.width() / max(1, g.width())
        sy = self.height() / max(1, g.height())
        return QRect(
            round((r.x() - g.x()) * sx), round((r.y() - g.y()) * sy), round(r.width() * sx), round(r.height() * sy)
        )

    def image_pos(self, p: QPoint) -> QPoint:
        return QPoint(
            int(p.x() * self.image.width() / max(1, self.width())),
            int(p.y() * self.image.height() / max(1, self.height())),
        )

    def color_at(self, p: QPoint) -> QColor:
        ip = self.image_pos(p)
        if 0 <= ip.x() < self.image.width() and 0 <= ip.y() < self.image.height():
            return self.image.pixelColor(ip)
        return QColor(0, 0, 0)

    # 표시 -----------------------------------------------------------------
    def start(self) -> None:
        if is_wayland():
            self.showFullScreen()
        else:
            self.setGeometry(self.snapshot.geometry)
            self.show()
        self.raise_()
        self.activateWindow()
        self.setFocus()
        QTimer.singleShot(50, self._grab_input)

    def _grab_input(self) -> None:
        self.activateWindow()
        self.setFocus()

    def paint_background(self, painter: QPainter) -> None:
        painter.drawPixmap(self.rect(), self.snapshot.pixmap)

    def draw_magnifier(self, painter: QPainter, extra_lines: list[str] | None = None) -> None:
        if not self.show_magnifier or not self.rect().contains(self.mouse):
            return
        cells = 15
        box = cells * self.zoom
        ip = self.image_pos(self.mouse)
        src = QRect(ip.x() - cells // 2, ip.y() - cells // 2, cells, cells)
        color = self.color_at(self.mouse)
        gp = self.to_global(self.mouse)
        lines = [f"{int(gp.x())}, {int(gp.y())}", f"RGB({color.red()}, {color.green()}, {color.blue()})  {color.name().upper()}"]
        if extra_lines:
            lines.extend(extra_lines)
        text_h = 18 * len(lines) + 6
        x = self.mouse.x() + 24
        y = self.mouse.y() + 24
        if x + box > self.width():
            x = self.mouse.x() - 24 - box
        if y + box + text_h > self.height():
            y = self.mouse.y() - 24 - box - text_h
        target = QRect(x, y, box, box)
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, False)
        painter.fillRect(target.adjusted(-2, -2, 2, text_h + 2), QColor(30, 30, 30, 230))
        painter.drawImage(QRectF(target), self.image, QRectF(src))
        painter.setPen(QPen(QColor(47, 140, 255, 200), 1))
        mid = target.topLeft() + QPoint((cells // 2) * self.zoom, (cells // 2) * self.zoom)
        painter.drawRect(QRect(mid, QSize(self.zoom, self.zoom)))
        painter.drawLine(target.left(), mid.y() + self.zoom // 2, mid.x(), mid.y() + self.zoom // 2)
        painter.drawLine(mid.x() + self.zoom, mid.y() + self.zoom // 2, target.right(), mid.y() + self.zoom // 2)
        painter.drawLine(mid.x() + self.zoom // 2, target.top(), mid.x() + self.zoom // 2, mid.y())
        painter.drawLine(mid.x() + self.zoom // 2, mid.y() + self.zoom, mid.x() + self.zoom // 2, target.bottom())
        painter.setPen(QColor("white"))
        f = QFont()
        f.setPixelSize(12)
        painter.setFont(f)
        for i, line in enumerate(lines):
            painter.drawText(QRect(x + 4, y + box + 4 + i * 18, box + 120, 18), Qt.AlignmentFlag.AlignLeft, line)
        painter.restore()

    def draw_hint(self, painter: QPainter, text: str) -> None:
        f = QFont()
        f.setPixelSize(13)
        painter.setFont(f)
        metrics = painter.fontMetrics()
        w = metrics.horizontalAdvance(text) + 28
        screen = QGuiApplication.screenAt(self.mapToGlobal(QPoint(self.width() // 2, 10)))
        top_center = self.width() // 2
        if screen is not None and not is_wayland():
            sg = screen.geometry().translated(-self.snapshot.geometry.topLeft())
            top_center = sg.center().x()
        r = QRect(top_center - w // 2, 12, w, 30)
        if r.contains(self.mouse):
            r.moveTop(self.height() - 50)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(20, 20, 20, 210))
        painter.drawRoundedRect(r, 6, 6)
        painter.setPen(QColor("white"))
        painter.drawText(r, Qt.AlignmentFlag.AlignCenter, text)

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Escape:
            self.cancel()
            return
        step = 10 if event.modifiers() & Qt.KeyboardModifier.ShiftModifier else 1
        moves = {
            Qt.Key.Key_Left: QPoint(-step, 0),
            Qt.Key.Key_Right: QPoint(step, 0),
            Qt.Key.Key_Up: QPoint(0, -step),
            Qt.Key.Key_Down: QPoint(0, step),
        }
        if event.key() in moves:
            self.cursor().setPos(self.mapToGlobal(self.mouse + moves[event.key()]))
            return
        super().keyPressEvent(event)

    def cancel(self) -> None:
        self.cancelled.emit()
        self.close()


class CaptureOverlay(FrozenOverlay):
    """region / fixed / freehand / window 모드의 영역 선택기."""

    selected = Signal(QRect, object)  # 전역 논리 좌표 영역, 자유형 경로(QPainterPath | None)

    HINTS = {
        "region": "드래그하여 캡처할 영역을 지정하세요.  ESC: 취소   방향키: 1px 이동",
        "fixed": "클릭하여 고정된 영역을 캡처하세요.  휠: 크기 변경   ESC: 취소",
        "freehand": "마우스로 자유롭게 영역을 그리세요.  ESC: 취소",
        "window": "캡처할 윈도우를 클릭하세요. 드래그하면 영역을 지정합니다.  ESC: 취소",
    }

    def __init__(
        self,
        snapshot: Snapshot,
        mode: str = "region",
        magnifier: bool = True,
        zoom: int = 6,
        fixed_size: QSize | None = None,
        windows: list[tuple[QRect, str]] | None = None,
        show_hint: bool = True,
    ):
        super().__init__(snapshot, magnifier, zoom)
        self.mode = mode
        self.fixed_size = fixed_size or QSize(640, 480)
        self.windows = windows or []
        self.show_hint = show_hint
        self.origin: QPoint | None = None
        self.current = QRect()
        self.path = QPainterPath()
        self.dragging = False
        self.hover_window: QRect | None = None

    # 이벤트 ---------------------------------------------------------------
    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            if self.dragging:
                self.dragging = False
                self.origin = None
                self.current = QRect()
                self.path = QPainterPath()
                self.update()
            else:
                self.cancel()
            return
        if event.button() != Qt.MouseButton.LeftButton:
            return
        pos = event.position().toPoint()
        if self.mode == "fixed":
            self._finish(self._fixed_rect(pos))
            return
        self.origin = pos
        self.dragging = True
        if self.mode == "freehand":
            self.path = QPainterPath(QPointF(pos))
        self.current = QRect(pos, pos)
        self.update()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        self.mouse = event.position().toPoint()
        if self.dragging and self.origin is not None:
            if self.mode == "freehand":
                self.path.lineTo(QPointF(self.mouse))
                self.current = self.path.boundingRect().toAlignedRect()
            else:
                self.current = QRect(self.origin, self.mouse).normalized()
        elif self.mode == "window":
            self.hover_window = self._window_at(self.mouse)
        self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() != Qt.MouseButton.LeftButton or not self.dragging:
            return
        self.dragging = False
        if self.mode == "freehand":
            self.path.closeSubpath()
            rect = self.path.boundingRect().toAlignedRect()
            if rect.width() > 3 and rect.height() > 3:
                self._finish(rect, self.path)
            return
        rect = self.current.normalized()
        if rect.width() < 3 or rect.height() < 3:
            if self.mode == "window" and self.hover_window is not None:
                self._finish(self.hover_window)
                return
            self.current = QRect()
            self.update()
            return
        self._finish(rect)

    def wheelEvent(self, event: QWheelEvent) -> None:
        if self.mode != "fixed":
            return
        delta = 10 if event.angleDelta().y() > 0 else -10
        if event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
            self.fixed_size.setWidth(max(10, self.fixed_size.width() + delta))
        elif event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self.fixed_size.setHeight(max(10, self.fixed_size.height() + delta))
        else:
            self.fixed_size = QSize(max(10, self.fixed_size.width() + delta), max(10, self.fixed_size.height() + delta))
        self.update()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            if self.mode == "fixed":
                self._finish(self._fixed_rect(self.mouse))
                return
            if self.mode == "window" and self.hover_window is not None:
                self._finish(self.hover_window)
                return
        super().keyPressEvent(event)

    # 보조 -----------------------------------------------------------------
    def _fixed_rect(self, center: QPoint) -> QRect:
        g = self.snapshot.geometry
        sx = self.width() / max(1, g.width())
        sy = self.height() / max(1, g.height())
        w = round(self.fixed_size.width() * sx)
        h = round(self.fixed_size.height() * sy)
        r = QRect(center.x() - w // 2, center.y() - h // 2, w, h)
        if r.left() < 0:
            r.moveLeft(0)
        if r.top() < 0:
            r.moveTop(0)
        if r.right() > self.width() - 1:
            r.moveRight(self.width() - 1)
        if r.bottom() > self.height() - 1:
            r.moveBottom(self.height() - 1)
        return r

    def _window_at(self, p: QPoint) -> QRect | None:
        gp = self.to_global(p).toPoint()
        for rect, _title in self.windows:
            if rect.contains(gp):
                return self.from_global_rect(rect).intersected(self.rect())
        return None

    def _finish(self, rect: QRect, path: QPainterPath | None = None) -> None:
        rect = rect.intersected(self.rect())
        if rect.isEmpty():
            return
        global_rect = self.rect_to_global(rect)
        global_path = None
        if path is not None:
            g = self.snapshot.geometry
            sx = self.snapshot.scale_x * g.width() / max(1, self.width())
            sy = self.snapshot.scale_y * g.height() / max(1, self.height())
            from PySide6.QtGui import QTransform

            t = QTransform()
            t.scale(sx, sy)
            t.translate(-rect.x(), -rect.y())
            global_path = t.map(path)
        self.hide()
        self.selected.emit(global_rect, global_path)
        self.close()

    # 그리기 ---------------------------------------------------------------
    def paintEvent(self, event: QPaintEvent) -> None:
        painter = QPainter(self)
        self.paint_background(painter)
        sel = QRect()
        if self.mode == "fixed" and self.rect().contains(self.mouse):
            sel = self._fixed_rect(self.mouse)
        elif self.mode == "window" and not self.dragging and self.hover_window is not None:
            sel = self.hover_window
        elif not self.current.isEmpty():
            sel = self.current

        dim = QPainterPath()
        dim.addRect(QRectF(self.rect()))
        if self.mode == "freehand" and not self.path.isEmpty():
            dim = dim.subtracted(self.path)
        elif not sel.isEmpty():
            hole = QPainterPath()
            hole.addRect(QRectF(sel))
            dim = dim.subtracted(hole)
        painter.fillPath(dim, DIM)

        painter.setRenderHint(QPainter.RenderHint.Antialiasing, self.mode == "freehand")
        pen = QPen(ACCENT, 2)
        painter.setPen(pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        if self.mode == "freehand" and not self.path.isEmpty():
            painter.drawPath(self.path)
        elif not sel.isEmpty():
            painter.drawRect(sel.adjusted(0, 0, -1, -1))
            g = self.rect_to_global(sel)
            label = f"{g.width()} x {g.height()}"
            f = QFont()
            f.setPixelSize(12)
            painter.setFont(f)
            tw = painter.fontMetrics().horizontalAdvance(label) + 12
            lr = QRect(sel.left(), sel.top() - 22, tw, 20)
            if lr.top() < 0:
                lr.moveTop(sel.top() + 2)
            painter.fillRect(lr, QColor(20, 20, 20, 210))
            painter.setPen(QColor("white"))
            painter.drawText(lr, Qt.AlignmentFlag.AlignCenter, label)

        if not self.dragging and self.mode in ("region", "window", "freehand"):
            painter.setPen(QPen(QColor(255, 255, 255, 120), 1, Qt.PenStyle.DashLine))
            painter.drawLine(0, self.mouse.y(), self.width(), self.mouse.y())
            painter.drawLine(self.mouse.x(), 0, self.mouse.x(), self.height())

        if self.mode != "fixed":
            self.draw_magnifier(painter)
        if self.show_hint:
            self.draw_hint(painter, self.HINTS.get(self.mode, ""))
        painter.end()
