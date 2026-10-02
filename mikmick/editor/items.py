"""캔버스 위의 편집 가능한 개체(도형, 펜, 텍스트, 스탬프, 이미지)."""

from __future__ import annotations

import math

from PySide6.QtCore import QLineF, QPointF, QRectF, Qt
from PySide6.QtGui import (
    QBrush,
    QColor,
    QFont,
    QPainter,
    QPainterPath,
    QPainterPathStroker,
    QPen,
    QPixmap,
    QPolygonF,
)
from PySide6.QtWidgets import (
    QGraphicsItem,
    QGraphicsPathItem,
    QGraphicsPixmapItem,
    QGraphicsTextItem,
    QStyle,
    QStyleOptionGraphicsItem,
)

OBJECT_FLAGS = (
    QGraphicsItem.GraphicsItemFlag.ItemIsSelectable
    | QGraphicsItem.GraphicsItemFlag.ItemIsMovable
    | QGraphicsItem.GraphicsItemFlag.ItemSendsGeometryChanges
)

SHAPE_KINDS = ["rect", "rounded_rect", "ellipse", "line", "arrow", "balloon"]
SHAPE_LABELS = {
    "rect": "사각형",
    "rounded_rect": "둥근 사각형",
    "ellipse": "타원",
    "line": "직선",
    "arrow": "화살표",
    "balloon": "말풍선",
}


def _draw_selection(painter: QPainter, rect: QRectF) -> None:
    painter.save()
    painter.setBrush(Qt.BrushStyle.NoBrush)
    painter.setPen(QPen(QColor(47, 140, 255), 1, Qt.PenStyle.DashLine))
    painter.drawRect(rect)
    painter.restore()


class ShapeItem(QGraphicsItem):
    def __init__(self, kind: str, p1: QPointF, p2: QPointF, color: QColor, width: int, fill: QColor | None = None):
        super().__init__()
        self.kind = kind
        self.p1 = QPointF(p1)
        self.p2 = QPointF(p2)
        self.color = QColor(color)
        self.width = width
        self.fill = QColor(fill) if fill is not None else None
        self.setFlags(OBJECT_FLAGS)

    def set_end(self, p2: QPointF) -> None:
        self.prepareGeometryChange()
        self.p2 = QPointF(p2)
        self.update()

    def _rect(self) -> QRectF:
        return QRectF(self.p1, self.p2).normalized()

    def _head_size(self) -> float:
        return max(10.0, self.width * 4.0)

    def boundingRect(self) -> QRectF:
        m = self.width + (self._head_size() if self.kind == "arrow" else 2) + 4
        r = self._rect()
        if self.kind == "balloon":
            r = r.adjusted(0, 0, 0, r.height() * 0.4)
        return r.adjusted(-m, -m, m, m)

    def shape(self) -> QPainterPath:
        path = QPainterPath()
        if self.kind in ("line", "arrow"):
            path.moveTo(self.p1)
            path.lineTo(self.p2)
            stroker = QPainterPathStroker()
            stroker.setWidth(max(8, self.width + 6))
            return stroker.createStroke(path)
        path.addRect(self.boundingRect())
        return path

    def _balloon_path(self) -> QPainterPath:
        r = self._rect()
        body = QPainterPath()
        body.addRoundedRect(r, min(16, r.width() / 4), min(16, r.height() / 4))
        tail = QPainterPath()
        tx = r.left() + r.width() * 0.25
        tail.moveTo(tx, r.bottom() - 1)
        tail.lineTo(tx - r.width() * 0.05, r.bottom() + r.height() * 0.4)
        tail.lineTo(tx + r.width() * 0.18, r.bottom() - 1)
        tail.closeSubpath()
        return body.united(tail)

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget=None) -> None:
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        pen = QPen(self.color, self.width)
        pen.setJoinStyle(Qt.PenJoinStyle.RoundJoin)
        pen.setCapStyle(Qt.PenCapStyle.RoundCap)
        painter.setPen(pen)
        painter.setBrush(QBrush(self.fill) if self.fill is not None else Qt.BrushStyle.NoBrush)
        r = self._rect()
        if self.kind == "rect":
            painter.drawRect(r)
        elif self.kind == "rounded_rect":
            rad = min(18.0, min(r.width(), r.height()) / 4)
            painter.drawRoundedRect(r, rad, rad)
        elif self.kind == "ellipse":
            painter.drawEllipse(r)
        elif self.kind == "balloon":
            painter.drawPath(self._balloon_path())
        elif self.kind == "line":
            painter.drawLine(self.p1, self.p2)
        elif self.kind == "arrow":
            line = QLineF(self.p1, self.p2)
            head = self._head_size()
            if line.length() > 1:
                ang = math.atan2(line.dy(), line.dx())
                base = QPointF(self.p2.x() - head * 0.8 * math.cos(ang), self.p2.y() - head * 0.8 * math.sin(ang))
                painter.drawLine(self.p1, base)
                a1, a2 = ang + math.radians(155), ang - math.radians(155)
                poly = QPolygonF(
                    [
                        self.p2,
                        QPointF(self.p2.x() + head * math.cos(a1), self.p2.y() + head * math.sin(a1)),
                        QPointF(self.p2.x() + head * math.cos(a2), self.p2.y() + head * math.sin(a2)),
                    ]
                )
                painter.setPen(QPen(self.color, 1, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
                painter.setBrush(self.color)
                painter.drawPolygon(poly)
        if option.state & QStyle.StateFlag.State_Selected:
            _draw_selection(painter, self.boundingRect())


class StrokeItem(QGraphicsPathItem):
    """연필 / 형광펜 / 지우개 자유 곡선."""

    def __init__(self, start: QPointF, color: QColor, width: int, highlighter: bool = False):
        super().__init__()
        self.setPath(QPainterPath(start))
        pen = QPen(color, width)
        pen.setCapStyle(Qt.PenCapStyle.SquareCap if highlighter else Qt.PenCapStyle.RoundCap)
        pen.setJoinStyle(Qt.PenJoinStyle.RoundJoin)
        self.setPen(pen)
        self.highlighter = highlighter
        self.setFlags(OBJECT_FLAGS)

    def add_point(self, p: QPointF) -> None:
        path = self.path()
        path.lineTo(p)
        self.setPath(path)

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget=None) -> None:
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        if self.highlighter:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Multiply)
        painter.setPen(self.pen())
        painter.setBrush(Qt.BrushStyle.NoBrush)
        path = self.path()
        if path.elementCount() <= 1:
            painter.drawPoint(path.currentPosition())
        else:
            painter.drawPath(path)
        if option.state & QStyle.StateFlag.State_Selected:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            _draw_selection(painter, self.boundingRect())


class TextItem(QGraphicsTextItem):
    def __init__(self, text: str, font: QFont, color: QColor):
        super().__init__(text)
        self.setFont(font)
        self.setDefaultTextColor(color)
        self.setFlags(OBJECT_FLAGS)

    def start_editing(self) -> None:
        self.setTextInteractionFlags(Qt.TextInteractionFlag.TextEditorInteraction)
        self.setFocus(Qt.FocusReason.MouseFocusReason)
        cursor = self.textCursor()
        cursor.select(cursor.SelectionType.Document)
        self.setTextCursor(cursor)

    def mouseDoubleClickEvent(self, event) -> None:
        self.start_editing()
        super().mouseDoubleClickEvent(event)

    def focusOutEvent(self, event) -> None:
        self.setTextInteractionFlags(Qt.TextInteractionFlag.NoTextInteraction)
        cursor = self.textCursor()
        cursor.clearSelection()
        self.setTextCursor(cursor)
        super().focusOutEvent(event)
        if not self.toPlainText().strip() and self.scene() is not None:
            self.scene().removeItem(self)


class StampItem(QGraphicsItem):
    """번호 스탬프 / 커서 스탬프."""

    def __init__(self, kind: str, number: int, color: QColor, size: int = 28):
        super().__init__()
        self.kind = kind
        self.number = number
        self.color = QColor(color)
        self.size = size
        self.setFlags(OBJECT_FLAGS)

    def boundingRect(self) -> QRectF:
        s = self.size
        if self.kind == "cursor":
            return QRectF(-2, -2, s * 0.75 + 4, s * 1.1 + 4)
        return QRectF(-s / 2 - 2, -s / 2 - 2, s + 4, s + 4)

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget=None) -> None:
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        s = self.size
        if self.kind == "cursor":
            k = s / 20.0
            poly = QPolygonF(
                [QPointF(0, 0), QPointF(0, 17 * k), QPointF(4 * k, 13 * k), QPointF(7 * k, 20 * k), QPointF(10 * k, 19 * k), QPointF(7 * k, 12 * k), QPointF(12 * k, 12 * k)]
            )
            painter.setPen(QPen(QColor("black"), 1.2))
            painter.setBrush(QColor("white"))
            painter.drawPolygon(poly)
        else:
            painter.setPen(QPen(self.color.darker(130), 1.5))
            painter.setBrush(self.color)
            painter.drawEllipse(QRectF(-s / 2, -s / 2, s, s))
            font = QFont()
            font.setBold(True)
            font.setPixelSize(int(s * (0.6 if self.number < 10 else 0.5)))
            painter.setFont(font)
            lum = 0.299 * self.color.red() + 0.587 * self.color.green() + 0.114 * self.color.blue()
            painter.setPen(QColor("black") if lum > 170 else QColor("white"))
            painter.drawText(QRectF(-s / 2, -s / 2, s, s), Qt.AlignmentFlag.AlignCenter, str(self.number))
        if option.state & QStyle.StateFlag.State_Selected:
            _draw_selection(painter, self.boundingRect())


class PixmapObject(QGraphicsPixmapItem):
    def __init__(self, pixmap: QPixmap):
        super().__init__(pixmap)
        self.setFlags(OBJECT_FLAGS)
        self.setTransformationMode(Qt.TransformationMode.SmoothTransformation)

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget=None) -> None:
        selected = option.state & QStyle.StateFlag.State_Selected
        option.state &= ~QStyle.StateFlag.State_Selected
        super().paint(painter, option, widget)
        if selected:
            _draw_selection(painter, self.boundingRect())
