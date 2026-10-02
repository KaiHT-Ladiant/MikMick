"""QPainter 로 직접 그리는 벡터 아이콘 모음 (아이콘 테마에 의존하지 않음)."""

from __future__ import annotations

import math
from collections.abc import Callable

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import (
    QBrush,
    QColor,
    QFont,
    QIcon,
    QLinearGradient,
    QPainter,
    QPainterPath,
    QPen,
    QPixmap,
    QPolygonF,
)

INK = QColor("#3d3d3d")
BLUE = QColor("#2f6db5")
LIGHT_BLUE = QColor("#9cc3ea")
RED = QColor("#d9452b")
ORANGE = QColor("#e8892c")

_cache: dict[tuple[str, int], QIcon] = {}
_painters: dict[str, Callable[[QPainter], None]] = {}


def _register(name: str):
    def deco(fn: Callable[[QPainter], None]):
        _painters[name] = fn
        return fn

    return deco


def _pen(p: QPainter, color: QColor = INK, width: float = 3.0) -> None:
    pen = QPen(color, width)
    pen.setJoinStyle(Qt.PenJoinStyle.RoundJoin)
    pen.setCapStyle(Qt.PenCapStyle.RoundCap)
    p.setPen(pen)


def _arrow_head(p: QPainter, tip: QPointF, angle: float, size: float = 10.0) -> None:
    a1 = angle + math.radians(150)
    a2 = angle - math.radians(150)
    poly = QPolygonF(
        [
            tip,
            QPointF(tip.x() + size * math.cos(a1), tip.y() + size * math.sin(a1)),
            QPointF(tip.x() + size * math.cos(a2), tip.y() + size * math.sin(a2)),
        ]
    )
    p.drawPolygon(poly)


def render(name: str, size: int = 64) -> QPixmap:
    pm = QPixmap(size, size)
    pm.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pm)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.scale(size / 64.0, size / 64.0)
    fn = _painters.get(name)
    if fn is None:
        _pen(painter)
        painter.drawRect(QRectF(12, 12, 40, 40))
    else:
        fn(painter)
    painter.end()
    return pm


def icon(name: str) -> QIcon:
    key = (name, 0)
    cached = _cache.get(key)
    if cached is not None:
        return cached
    ic = QIcon()
    for size in (16, 24, 32, 48, 64):
        ic.addPixmap(render(name, size))
    _cache[key] = ic
    return ic


# --- 앱 로고 -----------------------------------------------------------------


@_register("logo")
def _logo(p: QPainter) -> None:
    colors = ["#ff5f6d", "#ffa94d", "#38c2a4", "#4c7ef3"]
    p.translate(-3.5, 0)
    p.setPen(Qt.PenStyle.NoPen)
    for i, c in enumerate(colors):
        path = QPainterPath()
        x = 6 + i * 13
        if i % 2 == 0:
            path.moveTo(x, 58)
            path.lineTo(x + 10, 58)
            path.lineTo(x + 20, 6)
            path.lineTo(x + 10, 6)
        else:
            path.moveTo(x + 10, 58)
            path.lineTo(x + 20, 58)
            path.lineTo(x + 10, 6)
            path.lineTo(x, 6)
        path.closeSubpath()
        p.setBrush(QColor(c))
        p.drawPath(path)


# --- 파일 / 공통 -------------------------------------------------------------


@_register("new")
def _new(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    path = QPainterPath()
    path.moveTo(16, 6)
    path.lineTo(38, 6)
    path.lineTo(50, 18)
    path.lineTo(50, 58)
    path.lineTo(16, 58)
    path.closeSubpath()
    p.drawPath(path)
    p.drawPolyline(QPolygonF([QPointF(38, 6), QPointF(38, 18), QPointF(50, 18)]))


@_register("open")
def _open(p: QPainter) -> None:
    _pen(p, QColor("#b27b16"))
    p.setBrush(QColor("#f6c95c"))
    p.drawRoundedRect(QRectF(6, 14, 46, 38), 3, 3)
    p.setBrush(QColor("#fbe19c"))
    path = QPainterPath()
    path.moveTo(14, 26)
    path.lineTo(58, 26)
    path.lineTo(50, 52)
    path.lineTo(6, 52)
    path.closeSubpath()
    p.drawPath(path)
    _pen(p, BLUE, 3.5)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawArc(QRectF(30, 30, 18, 18), 30 * 16, 260 * 16)


@_register("save")
def _save(p: QPainter) -> None:
    _pen(p, BLUE)
    p.setBrush(QColor("#dbe8f7"))
    p.drawRoundedRect(QRectF(8, 8, 48, 48), 4, 4)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(18, 8, 28, 16))
    p.drawRect(QRectF(16, 34, 32, 22))


@_register("save_as")
def _save_as(p: QPainter) -> None:
    _save(p)
    _pen(p, ORANGE, 4)
    p.drawLine(QPointF(40, 56), QPointF(58, 38))


@_register("print")
def _print(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(18, 6, 28, 18))
    p.setBrush(QColor("#c9c9c9"))
    p.drawRoundedRect(QRectF(6, 22, 52, 24), 4, 4)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(18, 38, 28, 20))


@_register("undo")
def _undo(p: QPainter) -> None:
    _pen(p, BLUE, 4)
    p.setBrush(Qt.BrushStyle.NoBrush)
    path = QPainterPath()
    path.moveTo(16, 26)
    path.cubicTo(30, 12, 54, 18, 54, 38)
    p.drawPath(path)
    p.setBrush(BLUE)
    _arrow_head(p, QPointF(12, 30), math.radians(150), 14)


@_register("redo")
def _redo(p: QPainter) -> None:
    p.translate(64, 0)
    p.scale(-1, 1)
    _undo(p)


@_register("close")
def _close(p: QPainter) -> None:
    _pen(p, INK, 4)
    p.drawLine(QPointF(16, 16), QPointF(48, 48))
    p.drawLine(QPointF(48, 16), QPointF(16, 48))


@_register("back")
def _back(p: QPainter) -> None:
    _pen(p, QColor("white"), 3.5)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawEllipse(QRectF(5, 5, 54, 54))
    _pen(p, QColor("white"), 5)
    p.drawLine(QPointF(18, 32), QPointF(46, 32))
    p.drawPolyline(QPolygonF([QPointF(30, 20), QPointF(18, 32), QPointF(30, 44)]))


@_register("heart")
def _heart(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(QColor("#e2603f"))
    path = QPainterPath()
    path.moveTo(32, 56)
    path.cubicTo(4, 36, 4, 10, 22, 10)
    path.cubicTo(28, 10, 32, 16, 32, 20)
    path.cubicTo(32, 16, 36, 10, 42, 10)
    path.cubicTo(60, 10, 60, 36, 32, 56)
    p.drawPath(path)


@_register("options")
def _options(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.setBrush(QColor("#d8d8d8"))
    path = QPainterPath()
    for i in range(16):
        ang = math.radians(i * 22.5)
        r = 26 if i % 2 == 0 else 20
        pt = QPointF(32 + r * math.cos(ang), 32 + r * math.sin(ang))
        if i == 0:
            path.moveTo(pt)
        else:
            path.lineTo(pt)
    path.closeSubpath()
    p.drawPath(path)
    p.setBrush(QColor("white"))
    p.drawEllipse(QPointF(32, 32), 8, 8)


@_register("info")
def _info(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(BLUE)
    p.drawEllipse(QRectF(6, 6, 52, 52))
    p.setPen(QColor("white"))
    f = QFont()
    f.setPixelSize(36)
    f.setBold(True)
    p.setFont(f)
    p.drawText(QRectF(6, 6, 52, 52), Qt.AlignmentFlag.AlignCenter, "?")


@_register("update")
def _update(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(BLUE)
    p.drawEllipse(QRectF(6, 6, 52, 52))
    _pen(p, QColor("white"), 5)
    p.drawLine(QPointF(32, 16), QPointF(32, 44))
    p.drawPolyline(QPolygonF([QPointF(20, 34), QPointF(32, 46), QPointF(44, 34)]))


@_register("share")
def _share(p: QPainter) -> None:
    _pen(p, BLUE, 3.5)
    p.setBrush(LIGHT_BLUE)
    for c in (QPointF(46, 14), QPointF(16, 32), QPointF(46, 50)):
        p.drawEllipse(c, 8, 8)
    p.drawLine(QPointF(23, 28), QPointF(39, 18))
    p.drawLine(QPointF(23, 36), QPointF(39, 46))


@_register("thumbnail")
def _thumbnail(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    for x, y in ((8, 8), (34, 8), (8, 34), (34, 34)):
        p.drawRect(QRectF(x, y, 22, 22))


@_register("email")
def _email(p: QPainter) -> None:
    _pen(p, BLUE)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(6, 14, 52, 36))
    p.drawPolyline(QPolygonF([QPointF(6, 14), QPointF(32, 36), QPointF(58, 14)]))


@_register("ftp")
def _ftp(p: QPainter) -> None:
    _pen(p, BLUE)
    p.setBrush(LIGHT_BLUE)
    p.drawEllipse(QRectF(6, 6, 52, 52))
    p.drawEllipse(QRectF(20, 6, 24, 52))
    p.drawLine(QPointF(6, 32), QPointF(58, 32))


@_register("program")
def _program(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(6, 10, 52, 44))
    p.setBrush(BLUE)
    p.drawRect(QRectF(6, 10, 52, 10))
    _pen(p, ORANGE, 4)
    p.drawLine(QPointF(22, 44), QPointF(42, 30))
    p.setBrush(ORANGE)
    _arrow_head(p, QPointF(44, 28), math.radians(-35), 10)


@_register("clipboard_copy")
def _clipboard_copy(p: QPainter) -> None:
    _copy(p)


# --- 클립보드 ----------------------------------------------------------------


@_register("paste")
def _paste(p: QPainter) -> None:
    _pen(p, QColor("#8a6a2e"))
    p.setBrush(QColor("#e9c88a"))
    p.drawRoundedRect(QRectF(10, 10, 40, 48), 3, 3)
    p.setBrush(QColor("#9a9a9a"))
    p.drawRoundedRect(QRectF(22, 5, 16, 10), 2, 2)
    _pen(p)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(28, 26, 28, 32))


@_register("cut")
def _cut(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.drawLine(QPointF(20, 6), QPointF(42, 42))
    p.drawLine(QPointF(44, 6), QPointF(22, 42))
    p.setBrush(Qt.BrushStyle.NoBrush)
    _pen(p, RED, 4)
    p.drawEllipse(QPointF(18, 48), 9, 9)
    p.drawEllipse(QPointF(46, 48), 9, 9)


@_register("copy")
def _copy(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(8, 6, 30, 38))
    p.drawRect(QRectF(26, 20, 30, 38))


# --- 이미지 ------------------------------------------------------------------


@_register("effect")
def _effect(p: QPainter) -> None:
    _pen(p, INK, 4)
    p.drawLine(QPointF(10, 54), QPointF(40, 24))
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(ORANGE)
    for cx, cy, r in ((46, 14, 5), (54, 28, 3.5), (34, 10, 3), (52, 44, 2.5)):
        p.drawEllipse(QPointF(cx, cy), r, r)


@_register("resize")
def _resize(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    pen = p.pen()
    pen.setStyle(Qt.PenStyle.DashLine)
    p.setPen(pen)
    p.drawRect(QRectF(8, 8, 48, 48))
    _pen(p, BLUE, 3)
    p.setBrush(LIGHT_BLUE)
    p.drawRect(QRectF(8, 26, 30, 30))
    p.drawLine(QPointF(30, 34), QPointF(52, 12))
    p.setBrush(BLUE)
    _arrow_head(p, QPointF(54, 10), math.radians(-45), 10)


@_register("rotate")
def _rotate(p: QPainter) -> None:
    _pen(p)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(8, 24, 26, 32))
    _pen(p, BLUE, 4)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawArc(QRectF(20, 8, 36, 36), 0, 150 * 16)
    p.setBrush(BLUE)
    _arrow_head(p, QPointF(56, 28), math.radians(90), 12)


@_register("crop")
def _crop(p: QPainter) -> None:
    _pen(p, INK, 4)
    p.drawPolyline(QPolygonF([QPointF(16, 4), QPointF(16, 48), QPointF(60, 48)]))
    p.drawPolyline(QPolygonF([QPointF(4, 16), QPointF(48, 16), QPointF(48, 60)]))


# --- 도구 --------------------------------------------------------------------


@_register("move")
def _move(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("white"))
    poly = QPolygonF(
        [QPointF(8, 6), QPointF(8, 44), QPointF(18, 35), QPointF(26, 52), QPointF(32, 49), QPointF(24, 33), QPointF(37, 33)]
    )
    p.drawPolygon(poly)
    _pen(p, BLUE, 3)
    p.drawLine(QPointF(40, 46), QPointF(60, 46))
    p.drawLine(QPointF(50, 36), QPointF(50, 58))


@_register("select")
def _select(p: QPainter) -> None:
    pen = QPen(INK, 3, Qt.PenStyle.DashLine)
    p.setPen(pen)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawRect(QRectF(8, 12, 48, 40))


@_register("draw")
def _draw(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("#f2b134"))
    path = QPainterPath()
    path.moveTo(44, 6)
    path.lineTo(58, 20)
    path.lineTo(24, 54)
    path.lineTo(10, 40)
    path.closeSubpath()
    p.drawPath(path)
    p.setBrush(INK)
    p.drawPolygon(QPolygonF([QPointF(10, 40), QPointF(24, 54), QPointF(6, 58)]))


@_register("highlighter")
def _highlighter(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(QColor(255, 230, 0, 170))
    p.drawRect(QRectF(4, 40, 56, 16))
    _pen(p, INK, 2.5)
    p.setBrush(QColor("#ffe600"))
    path = QPainterPath()
    path.moveTo(40, 6)
    path.lineTo(56, 22)
    path.lineTo(32, 46)
    path.lineTo(16, 30)
    path.closeSubpath()
    p.drawPath(path)


@_register("eraser")
def _eraser(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("#f1a3b3"))
    path = QPainterPath()
    path.moveTo(36, 8)
    path.lineTo(58, 30)
    path.lineTo(32, 56)
    path.lineTo(10, 34)
    path.closeSubpath()
    p.drawPath(path)
    p.drawLine(QPointF(22, 22), QPointF(44, 44))


@_register("fill")
def _fill(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("white"))
    path = QPainterPath()
    path.moveTo(26, 8)
    path.lineTo(48, 30)
    path.lineTo(28, 50)
    path.lineTo(6, 28)
    path.closeSubpath()
    p.drawPath(path)
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(BLUE)
    path2 = QPainterPath()
    path2.moveTo(54, 36)
    path2.cubicTo(60, 46, 60, 54, 54, 54)
    path2.cubicTo(48, 54, 48, 46, 54, 36)
    p.drawPath(path2)


@_register("text")
def _text(p: QPainter) -> None:
    f = QFont("Serif")
    f.setPixelSize(54)
    f.setBold(True)
    p.setFont(f)
    p.setPen(INK)
    p.drawText(QRectF(0, 0, 64, 64), Qt.AlignmentFlag.AlignCenter, "T")


@_register("stamp")
def _stamp(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("#9a9a9a"))
    p.drawEllipse(QRectF(22, 4, 20, 20))
    p.drawRect(QRectF(27, 22, 10, 14))
    p.setBrush(QColor("#c9c9c9"))
    p.drawRoundedRect(QRectF(8, 34, 48, 14), 3, 3)
    p.drawRect(QRectF(12, 48, 40, 8))


@_register("number_stamp")
def _number_stamp(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(RED)
    p.drawEllipse(QRectF(6, 6, 52, 52))
    p.setPen(QColor("white"))
    f = QFont()
    f.setPixelSize(34)
    f.setBold(True)
    p.setFont(f)
    p.drawText(QRectF(6, 6, 52, 52), Qt.AlignmentFlag.AlignCenter, "1")


@_register("cursor_stamp")
def _cursor_stamp(p: QPainter) -> None:
    _move_cursor_only(p)


def _move_cursor_only(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("white"))
    poly = QPolygonF(
        [QPointF(16, 6), QPointF(16, 52), QPointF(28, 41), QPointF(37, 60), QPointF(44, 57), QPointF(35, 38), QPointF(51, 38)]
    )
    p.drawPolygon(poly)


@_register("shape")
def _shape(p: QPainter) -> None:
    _pen(p, BLUE, 3)
    p.setBrush(LIGHT_BLUE)
    p.drawRect(QRectF(6, 22, 32, 32))
    p.setBrush(QColor(255, 255, 255, 200))
    p.drawEllipse(QRectF(26, 6, 32, 32))


@_register("rect")
def _rect(p: QPainter) -> None:
    _pen(p, INK, 3.5)
    p.drawRect(QRectF(8, 14, 48, 36))


@_register("rounded_rect")
def _rounded_rect(p: QPainter) -> None:
    _pen(p, INK, 3.5)
    p.drawRoundedRect(QRectF(8, 14, 48, 36), 10, 10)


@_register("ellipse")
def _ellipse(p: QPainter) -> None:
    _pen(p, INK, 3.5)
    p.drawEllipse(QRectF(6, 14, 52, 36))


@_register("line")
def _line(p: QPainter) -> None:
    _pen(p, INK, 4)
    p.drawLine(QPointF(8, 56), QPointF(56, 8))


@_register("arrow")
def _arrow(p: QPainter) -> None:
    _pen(p, RED, 4.5)
    p.drawLine(QPointF(8, 56), QPointF(50, 14))
    p.setBrush(RED)
    _arrow_head(p, QPointF(56, 8), math.radians(-45), 16)


@_register("balloon")
def _balloon(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.setBrush(QColor("white"))
    path = QPainterPath()
    path.addRoundedRect(QRectF(6, 8, 52, 34), 8, 8)
    tail = QPainterPath()
    tail.moveTo(18, 40)
    tail.lineTo(14, 58)
    tail.lineTo(30, 40)
    tail.closeSubpath()
    p.drawPath(path.united(tail))


@_register("mosaic")
def _mosaic(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    shades = ["#555", "#999", "#ccc", "#777"]
    for i in range(4):
        for j in range(4):
            p.setBrush(QColor(shades[(i + j * 3) % 4]))
            p.drawRect(QRectF(8 + i * 12, 8 + j * 12, 12, 12))


@_register("blur")
def _blur(p: QPainter) -> None:
    grad = QLinearGradient(0, 0, 64, 64)
    grad.setColorAt(0, QColor("#ffffff"))
    grad.setColorAt(1, QColor("#5c8fd1"))
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(QBrush(grad))
    p.drawEllipse(QRectF(8, 8, 48, 48))


@_register("eyedropper")
def _eyedropper(p: QPainter) -> None:
    _pen(p, INK, 2.5)
    p.setBrush(QColor("#5a5a5a"))
    path = QPainterPath()
    path.moveTo(44, 6)
    path.lineTo(58, 20)
    path.lineTo(50, 26)
    path.lineTo(38, 14)
    path.closeSubpath()
    p.drawPath(path)
    p.setBrush(LIGHT_BLUE)
    path2 = QPainterPath()
    path2.moveTo(40, 18)
    path2.lineTo(46, 24)
    path2.lineTo(18, 52)
    path2.lineTo(10, 54)
    path2.lineTo(12, 46)
    path2.closeSubpath()
    p.drawPath(path2)


@_register("swap")
def _swap(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawArc(QRectF(12, 12, 40, 40), 30 * 16, 120 * 16)
    p.drawArc(QRectF(12, 12, 40, 40), 210 * 16, 120 * 16)
    p.setBrush(INK)
    _arrow_head(p, QPointF(49, 22), math.radians(60), 9)
    _arrow_head(p, QPointF(15, 42), math.radians(240), 9)


@_register("reset_colors")
def _reset_colors(p: QPainter) -> None:
    _pen(p, INK, 2)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(24, 24, 28, 28))
    p.setBrush(QColor("black"))
    p.drawRect(QRectF(12, 12, 28, 28))


@_register("more_colors")
def _more_colors(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    for i, c in enumerate(["#e74c3c", "#f1c40f", "#2ecc71", "#3498db", "#9b59b6", "#95a5a6"]):
        ang = math.radians(i * 60 - 90)
        p.setBrush(QColor(c))
        p.drawEllipse(QPointF(32 + 18 * math.cos(ang), 32 + 18 * math.sin(ang)), 8, 8)


@_register("line_width")
def _line_width(p: QPainter) -> None:
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(INK)
    y = 10
    for h in (2, 4, 6, 9):
        p.drawRect(QRectF(6, y, 52, h))
        y += h + 8


@_register("zoom_in")
def _zoom_in(p: QPainter) -> None:
    _magnifier(p)
    _pen(p, BLUE, 4)
    p.drawLine(QPointF(30, 24), QPointF(46, 24))
    p.drawLine(QPointF(38, 16), QPointF(38, 32))


@_register("zoom_out")
def _zoom_out(p: QPainter) -> None:
    _magnifier(p)
    _pen(p, BLUE, 4)
    p.drawLine(QPointF(30, 24), QPointF(46, 24))


@_register("zoom_fit")
def _zoom_fit(p: QPainter) -> None:
    _pen(p, INK, 3)
    for a, b, c in (
        ((8, 22), (8, 8), (22, 8)),
        ((42, 8), (56, 8), (56, 22)),
        ((56, 42), (56, 56), (42, 56)),
        ((22, 56), (8, 56), (8, 42)),
    ):
        p.drawPolyline(QPolygonF([QPointF(*a), QPointF(*b), QPointF(*c)]))
    p.setBrush(LIGHT_BLUE)
    p.drawRect(QRectF(18, 18, 28, 28))


@_register("zoom_100")
def _zoom_100(p: QPainter) -> None:
    f = QFont()
    f.setPixelSize(22)
    f.setBold(True)
    p.setFont(f)
    p.setPen(INK)
    p.drawText(QRectF(0, 0, 64, 64), Qt.AlignmentFlag.AlignCenter, "1:1")


@_register("grid")
def _grid(p: QPainter) -> None:
    _pen(p, INK, 2)
    for i in range(5):
        v = 8 + i * 12
        p.drawLine(QPointF(v, 8), QPointF(v, 56))
        p.drawLine(QPointF(8, v), QPointF(56, v))


# --- 화면 캡처 도구 ----------------------------------------------------------


@_register("cap_fullscreen")
def _cap_fullscreen(p: QPainter) -> None:
    _pen(p, BLUE, 3)
    p.setBrush(QColor("white"))
    p.drawRoundedRect(QRectF(4, 8, 56, 38), 3, 3)
    p.drawLine(QPointF(32, 46), QPointF(32, 54))
    p.drawLine(QPointF(18, 56), QPointF(46, 56))


@_register("cap_window")
def _cap_window(p: QPainter) -> None:
    _pen(p, BLUE, 3)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(6, 10, 52, 44))
    p.setBrush(BLUE)
    p.drawRect(QRectF(6, 10, 52, 10))


@_register("cap_control")
def _cap_control(p: QPainter) -> None:
    _cap_window(p)
    _pen(p, INK, 3)
    p.setBrush(QColor(255, 255, 255, 220))
    p.drawEllipse(QPointF(42, 42), 9, 9)
    p.drawLine(QPointF(48, 48), QPointF(58, 58))


@_register("cap_scroll")
def _cap_scroll(p: QPainter) -> None:
    _pen(p, BLUE, 3)
    p.setBrush(QColor("white"))
    p.drawRect(QRectF(8, 6, 48, 52))
    for y in (16, 24, 32):
        p.drawLine(QPointF(16, y), QPointF(48, y))
    p.setBrush(BLUE)
    _pen(p, BLUE, 4)
    p.drawLine(QPointF(32, 30), QPointF(32, 50))
    _arrow_head(p, QPointF(32, 54), math.radians(90), 10)


@_register("cap_region")
def _cap_region(p: QPainter) -> None:
    pen = QPen(INK, 3, Qt.PenStyle.DashLine)
    p.setPen(pen)
    p.drawRect(QRectF(6, 6, 40, 40))
    _pen(p, BLUE, 4)
    p.drawLine(QPointF(46, 40), QPointF(46, 60))
    p.drawLine(QPointF(36, 50), QPointF(56, 50))


@_register("cap_fixed")
def _cap_fixed(p: QPainter) -> None:
    pen = QPen(RED, 3, Qt.PenStyle.DashLine)
    p.setPen(pen)
    p.drawRect(QRectF(10, 10, 44, 44))
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(RED)
    for x, y in ((10, 10), (54, 10), (10, 54), (54, 54), (32, 10), (32, 54), (10, 32), (54, 32)):
        p.drawRect(QRectF(x - 3, y - 3, 6, 6))


@_register("cap_freehand")
def _cap_freehand(p: QPainter) -> None:
    _pen(p, INK, 3)
    path = QPainterPath()
    path.moveTo(14, 40)
    path.cubicTo(0, 30, 10, 10, 26, 14)
    path.cubicTo(32, 2, 56, 6, 54, 22)
    path.cubicTo(62, 30, 52, 44, 40, 40)
    path.cubicTo(30, 48, 20, 46, 14, 40)
    p.drawPath(path)
    _pen(p, RED, 4)
    p.drawLine(QPointF(48, 42), QPointF(48, 60))
    p.drawLine(QPointF(39, 51), QPointF(57, 51))


@_register("cap_repeat")
def _cap_repeat(p: QPainter) -> None:
    _pen(p, BLUE, 5)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawArc(QRectF(10, 10, 44, 44), 100 * 16, 300 * 16)
    p.setBrush(BLUE)
    _pen(p, BLUE, 2)
    _arrow_head(p, QPointF(26, 8), math.radians(180), 14)


@_register("cap_delay")
def _cap_delay(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.setBrush(QColor("white"))
    p.drawEllipse(QRectF(8, 8, 48, 48))
    _pen(p, BLUE, 4)
    p.drawLine(QPointF(32, 32), QPointF(32, 16))
    p.drawLine(QPointF(32, 32), QPointF(44, 38))


# --- 그래픽 도구 -------------------------------------------------------------


@_register("tool_color_picker")
def _tool_color_picker(p: QPainter) -> None:
    _eyedropper(p)


@_register("tool_palette")
def _tool_palette(p: QPainter) -> None:
    p.setPen(QPen(INK, 1.5))
    colors = ["#d94a4a", "#4aa3d9", "#6fbf4a", "#e8a33c", "#8b5fbf", "#4a6ad9", "#bf4a8b", "#3cbfa0", "#d9d94a"]
    for i, c in enumerate(colors):
        p.setBrush(QColor(c))
        p.drawRect(QRectF(8 + (i % 3) * 16, 8 + (i // 3) * 16, 14, 14))


def _magnifier(p: QPainter) -> None:
    _pen(p, BLUE, 4)
    p.setBrush(QColor(255, 255, 255, 200))
    p.drawEllipse(QPointF(38, 24), 18, 18)
    _pen(p, BLUE, 7)
    p.drawLine(QPointF(25, 37), QPointF(8, 56))


@_register("tool_magnifier")
def _tool_magnifier(p: QPainter) -> None:
    _magnifier(p)


@_register("tool_ruler")
def _tool_ruler(p: QPainter) -> None:
    _pen(p, ORANGE, 3)
    p.setBrush(QColor("#fff3dc"))
    path = QPainterPath()
    path.moveTo(8, 6)
    path.lineTo(20, 6)
    path.lineTo(20, 46)
    path.lineTo(58, 46)
    path.lineTo(58, 58)
    path.lineTo(8, 58)
    path.closeSubpath()
    p.drawPath(path)
    _pen(p, ORANGE, 2)
    for y in range(12, 44, 6):
        p.drawLine(QPointF(8, y), QPointF(14, y))
    for x in range(26, 58, 6):
        p.drawLine(QPointF(x, 58), QPointF(x, 52))


@_register("tool_crosshair")
def _tool_crosshair(p: QPainter) -> None:
    _pen(p, INK, 3)
    p.setBrush(Qt.BrushStyle.NoBrush)
    p.drawEllipse(QPointF(32, 32), 20, 20)
    p.drawLine(QPointF(32, 4), QPointF(32, 60))
    p.drawLine(QPointF(4, 32), QPointF(60, 32))
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(RED)
    p.drawEllipse(QPointF(32, 32), 5, 5)


@_register("tool_protractor")
def _tool_protractor(p: QPainter) -> None:
    _pen(p, BLUE, 3)
    p.setBrush(LIGHT_BLUE)
    path = QPainterPath()
    path.moveTo(4, 48)
    path.arcTo(QRectF(4, 16, 56, 64), 180, -180)
    path.closeSubpath()
    p.drawPath(path)
    p.setBrush(QColor("white"))
    path2 = QPainterPath()
    path2.moveTo(20, 48)
    path2.arcTo(QRectF(20, 34, 24, 28), 180, -180)
    path2.closeSubpath()
    p.drawPath(path2)


@_register("tool_whiteboard")
def _tool_whiteboard(p: QPainter) -> None:
    _pen(p, INK, 2)
    p.setBrush(BLUE)
    path = QPainterPath()
    path.moveTo(48, 6)
    path.lineTo(58, 16)
    path.lineTo(22, 52)
    path.lineTo(12, 42)
    path.closeSubpath()
    p.drawPath(path)
    p.setBrush(INK)
    p.drawPolygon(QPolygonF([QPointF(12, 42), QPointF(22, 52), QPointF(6, 58)]))


@_register("tool_editor")
def _tool_editor(p: QPainter) -> None:
    _logo(p)


@_register("tray")
def _tray(p: QPainter) -> None:
    _logo(p)
