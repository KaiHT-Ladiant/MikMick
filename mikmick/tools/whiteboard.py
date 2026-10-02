"""프리젠테이션 도구: 바탕 화면 위에 직접 그리기."""

from __future__ import annotations

import math

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import (
    QColor,
    QKeyEvent,
    QMouseEvent,
    QPainter,
    QPainterPath,
    QPaintEvent,
    QPen,
    QPixmap,
    QPolygonF,
)
from PySide6.QtWidgets import QButtonGroup, QHBoxLayout, QLabel, QToolButton, QWidget

from mikmick.capture.backend import Snapshot, is_wayland, virtual_geometry
from mikmick.icons import icon

COLORS = ["#ff3b30", "#ffcc00", "#34c759", "#007aff", "#000000", "#ffffff"]


class Stroke:
    def __init__(self, kind: str, color: QColor, width: int, start: QPointF):
        self.kind = kind
        self.color = QColor(color)
        self.width = width
        self.points = [start]
        self.end = start

    def paint(self, p: QPainter) -> None:
        pen = QPen(self.color, self.width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin)
        p.setPen(pen)
        p.setBrush(Qt.BrushStyle.NoBrush)
        if self.kind in ("pen", "highlighter", "eraser"):
            if self.kind == "eraser":
                p.setCompositionMode(QPainter.CompositionMode.CompositionMode_Clear)
            path = QPainterPath(self.points[0])
            for pt in self.points[1:]:
                path.lineTo(pt)
            if len(self.points) == 1:
                path.lineTo(self.points[0] + QPointF(0.1, 0.1))
            p.drawPath(path)
            p.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
        elif self.kind == "line":
            p.drawLine(self.points[0], self.end)
        elif self.kind == "arrow":
            a, b = self.points[0], self.end
            p.drawLine(a, b)
            ang = math.atan2(b.y() - a.y(), b.x() - a.x())
            head = max(12, self.width * 4)
            poly = QPolygonF(
                [
                    b,
                    QPointF(b.x() + head * math.cos(ang + 2.7), b.y() + head * math.sin(ang + 2.7)),
                    QPointF(b.x() + head * math.cos(ang - 2.7), b.y() + head * math.sin(ang - 2.7)),
                ]
            )
            p.setBrush(self.color)
            p.drawPolygon(poly)
        elif self.kind == "rect":
            p.drawRect(QRectF(self.points[0], self.end).normalized())
        elif self.kind == "ellipse":
            p.drawEllipse(QRectF(self.points[0], self.end).normalized())


class Whiteboard(QWidget):
    TOOLS = [
        ("pen", "draw", "펜"),
        ("highlighter", "highlighter", "형광펜"),
        ("line", "line", "직선"),
        ("arrow", "arrow", "화살표"),
        ("rect", "rect", "사각형"),
        ("ellipse", "ellipse", "타원"),
        ("eraser", "eraser", "지우개"),
    ]

    def __init__(self, snapshot: Snapshot | None = None):
        super().__init__(None)
        self.snapshot = snapshot
        self.background = "screen" if snapshot is not None else "transparent"
        self.kind = "pen"
        self.color = QColor(COLORS[0])
        self.width_ = 4
        self.strokes: list[Stroke] = []
        self.redo_stack: list[Stroke] = []
        self.current: Stroke | None = None
        self.layer = QPixmap()
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.WindowStaysOnTopHint | Qt.WindowType.Tool)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.setCursor(Qt.CursorShape.CrossCursor)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self._build_toolbar()

    def _build_toolbar(self) -> None:
        bar = QWidget(self)
        bar.setObjectName("WbBar")
        bar.setStyleSheet(
            "#WbBar { background: rgba(32,32,32,230); border-radius: 8px; }"
            "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; color: white; padding: 3px; }"
            "QToolButton:hover { background: rgba(255,255,255,40); }"
            "QToolButton:checked { background: rgba(47,140,255,160); }"
            "QLabel { color: #ddd; }"
        )
        lay = QHBoxLayout(bar)
        lay.setContentsMargins(8, 4, 8, 4)
        lay.setSpacing(2)
        group = QButtonGroup(self)
        for key, ic, tip in self.TOOLS:
            b = QToolButton()
            b.setIcon(icon(ic))
            b.setToolTip(tip)
            b.setCheckable(True)
            b.setChecked(key == self.kind)
            b.clicked.connect(lambda _=False, k=key: setattr(self, "kind", k))
            group.addButton(b)
            lay.addWidget(b)
        lay.addSpacing(8)
        cgroup = QButtonGroup(self)
        for i, c in enumerate(COLORS):
            b = QToolButton()
            b.setCheckable(True)
            b.setChecked(i == 0)
            b.setFixedSize(22, 22)
            b.setStyleSheet(
                f"QToolButton {{ background: {c}; border: 2px solid #555; border-radius: 11px; }}"
                f"QToolButton:checked {{ border: 2px solid white; }}"
            )
            b.clicked.connect(lambda _=False, col=c: setattr(self, "color", QColor(col)))
            cgroup.addButton(b)
            lay.addWidget(b)
        lay.addSpacing(8)
        wgroup = QButtonGroup(self)
        for w in (2, 4, 8, 14):
            b = QToolButton()
            b.setText(str(w))
            b.setCheckable(True)
            b.setChecked(w == self.width_)
            b.clicked.connect(lambda _=False, v=w: setattr(self, "width_", v))
            wgroup.addButton(b)
            lay.addWidget(b)
        lay.addSpacing(8)
        for label, mode in (("투명", "transparent"), ("화면", "screen"), ("흰색", "white"), ("검정", "black")):
            b = QToolButton()
            b.setText(label)
            b.setToolTip("배경")
            b.clicked.connect(lambda _=False, m=mode: self._set_bg(m))
            lay.addWidget(b)
        lay.addSpacing(8)
        for ic, tip, slot in (("undo", "실행 취소 (Ctrl+Z)", self.undo), ("redo", "다시 실행 (Ctrl+Y)", self.redo)):
            b = QToolButton()
            b.setIcon(icon(ic))
            b.setToolTip(tip)
            b.clicked.connect(slot)
            lay.addWidget(b)
        clear = QToolButton()
        clear.setText("지우기")
        clear.clicked.connect(self.clear)
        lay.addWidget(clear)
        close = QToolButton()
        close.setText("닫기 (ESC)")
        close.clicked.connect(self.close)
        lay.addWidget(close)
        self.hint = QLabel("")
        lay.addWidget(self.hint)
        bar.adjustSize()
        self.bar = bar

    def _set_bg(self, mode: str) -> None:
        if mode == "screen" and self.snapshot is None:
            return
        self.background = mode
        self.update()

    def start(self) -> None:
        if is_wayland():
            self.showFullScreen()
        else:
            self.setGeometry(self.snapshot.geometry if self.snapshot else virtual_geometry(True))
            self.show()
        self.bar.move((self.width() - self.bar.width()) // 2, 12)
        self.raise_()
        self.activateWindow()
        self.setFocus()

    def resizeEvent(self, event) -> None:
        self.bar.move((self.width() - self.bar.width()) // 2, 12)
        super().resizeEvent(event)

    def undo(self) -> None:
        if self.strokes:
            self.redo_stack.append(self.strokes.pop())
            self.update()

    def redo(self) -> None:
        if self.redo_stack:
            self.strokes.append(self.redo_stack.pop())
            self.update()

    def clear(self) -> None:
        self.strokes.clear()
        self.redo_stack.clear()
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.RightButton:
            self.undo()
            return
        if event.button() != Qt.MouseButton.LeftButton:
            return
        color = QColor(self.color)
        width = self.width_
        if self.kind == "highlighter":
            color.setAlpha(110)
            width = self.width_ * 4
        elif self.kind == "eraser":
            width = self.width_ * 5
        self.current = Stroke(self.kind, color, width, event.position())
        self.redo_stack.clear()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self.current is None:
            return
        if self.current.kind in ("pen", "highlighter", "eraser"):
            self.current.points.append(event.position())
        else:
            self.current.end = event.position()
        self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if self.current is not None:
            self.strokes.append(self.current)
            self.current = None
            self.update()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        ctrl = event.modifiers() & Qt.KeyboardModifier.ControlModifier
        if event.key() == Qt.Key.Key_Escape:
            self.close()
        elif ctrl and event.key() == Qt.Key.Key_Z:
            self.undo()
        elif ctrl and event.key() == Qt.Key.Key_Y:
            self.redo()
        elif event.key() == Qt.Key.Key_Delete:
            self.clear()
        else:
            super().keyPressEvent(event)

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        if self.background == "screen" and self.snapshot is not None:
            p.drawPixmap(self.rect(), self.snapshot.pixmap)
        elif self.background == "white":
            p.fillRect(self.rect(), QColor("white"))
        elif self.background == "black":
            p.fillRect(self.rect(), QColor("#111"))
        else:
            # 완전 투명 픽셀은 일부 환경에서 마우스 입력이 통과하므로 alpha=1 로 채운다.
            p.fillRect(self.rect(), QColor(0, 0, 0, 1))
        if self.layer.size() != self.size():
            self.layer = QPixmap(self.size())
        self.layer.fill(Qt.GlobalColor.transparent)
        lp = QPainter(self.layer)
        lp.setRenderHint(QPainter.RenderHint.Antialiasing)
        for s in self.strokes + ([self.current] if self.current else []):
            s.paint(lp)
        lp.end()
        p.drawPixmap(0, 0, self.layer)
        p.end()
