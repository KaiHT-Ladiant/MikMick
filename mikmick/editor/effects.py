"""이미지 효과 및 필터 (QImage → QImage, numpy 기반)."""

from __future__ import annotations

import numpy as np
from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QImage, QPainter, QPainterPath, QPen, QTransform


def to_array(img: QImage) -> np.ndarray:
    """ARGB32(premultiplied 아님) → (h, w, 4) uint8 배열 [B, G, R, A]."""
    img = img.convertToFormat(QImage.Format.Format_ARGB32)
    w, h = img.width(), img.height()
    buf = np.frombuffer(img.constBits(), dtype=np.uint8, count=img.bytesPerLine() * h)
    return buf.reshape(h, img.bytesPerLine())[:, : w * 4].reshape(h, w, 4).copy()


def from_array(arr: np.ndarray) -> QImage:
    arr = np.ascontiguousarray(np.clip(arr, 0, 255).astype(np.uint8))
    h, w = arr.shape[:2]
    img = QImage(arr.data, w, h, w * 4, QImage.Format.Format_ARGB32)
    return img.copy()


def _apply_region(img: QImage, rect: QRect | None, fn) -> QImage:
    if rect is None or rect.isNull():
        return fn(img)
    rect = rect.intersected(img.rect())
    if rect.isEmpty():
        return img
    part = fn(img.copy(rect))
    out = img.convertToFormat(QImage.Format.Format_ARGB32)
    painter = QPainter(out)
    painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
    painter.drawImage(rect.topLeft(), part)
    painter.end()
    return out


def _box_blur_axis(a: np.ndarray, r: int, axis: int) -> np.ndarray:
    if r <= 0:
        return a
    pad = [(0, 0)] * a.ndim
    pad[axis] = (r + 1, r)
    padded = np.pad(a, pad, mode="edge")
    c = np.cumsum(padded, axis=axis, dtype=np.float64)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return (hi - lo) / (2 * r + 1)


def blur(img: QImage, radius: int = 4, rect: QRect | None = None) -> QImage:
    def fn(im: QImage) -> QImage:
        a = to_array(im).astype(np.float64)
        for _ in range(3):
            a = _box_blur_axis(a, radius, 0)
            a = _box_blur_axis(a, radius, 1)
        return from_array(a)

    return _apply_region(img, rect, fn)


def sharpen(img: QImage, amount: float = 1.0, rect: QRect | None = None) -> QImage:
    def fn(im: QImage) -> QImage:
        a = to_array(im).astype(np.float64)
        b = _box_blur_axis(_box_blur_axis(a, 1, 0), 1, 1)
        out = a + amount * (a - b)
        out[..., 3] = a[..., 3]
        return from_array(out)

    return _apply_region(img, rect, fn)


def mosaic(img: QImage, block: int = 10, rect: QRect | None = None) -> QImage:
    def fn(im: QImage) -> QImage:
        a = to_array(im)
        h, w = a.shape[:2]
        out = a.copy()
        for y in range(0, h, block):
            for x in range(0, w, block):
                cell = a[y : y + block, x : x + block]
                out[y : y + block, x : x + block] = cell.reshape(-1, 4).mean(axis=0)
        return from_array(out)

    return _apply_region(img, rect, fn)


def grayscale(img: QImage, rect: QRect | None = None) -> QImage:
    def fn(im: QImage) -> QImage:
        a = to_array(im).astype(np.float64)
        g = 0.114 * a[..., 0] + 0.587 * a[..., 1] + 0.299 * a[..., 2]
        a[..., 0] = a[..., 1] = a[..., 2] = g
        return from_array(a)

    return _apply_region(img, rect, fn)


def invert(img: QImage, rect: QRect | None = None) -> QImage:
    def fn(im: QImage) -> QImage:
        a = to_array(im)
        a[..., :3] = 255 - a[..., :3]
        return from_array(a)

    return _apply_region(img, rect, fn)


def brightness_contrast(img: QImage, brightness: int = 0, contrast: int = 0, rect: QRect | None = None) -> QImage:
    """brightness, contrast: -100 ~ 100."""

    def fn(im: QImage) -> QImage:
        a = to_array(im).astype(np.float64)
        rgb = a[..., :3] + brightness * 2.55
        factor = (259 * (contrast * 2.55 + 255)) / (255 * (259 - contrast * 2.55))
        rgb = factor * (rgb - 128) + 128
        a[..., :3] = rgb
        return from_array(a)

    return _apply_region(img, rect, fn)


def hue_saturation(img: QImage, hue: int = 0, saturation: int = 0, lightness: int = 0, rect: QRect | None = None) -> QImage:
    """hue: -180~180, saturation/lightness: -100~100."""

    def fn(im: QImage) -> QImage:
        a = to_array(im).astype(np.float64) / 255.0
        b, g, r = a[..., 0], a[..., 1], a[..., 2]
        mx = np.max(a[..., :3], axis=-1)
        mn = np.min(a[..., :3], axis=-1)
        light = (mx + mn) / 2
        d = mx - mn
        s = np.where(d == 0, 0, d / (1 - np.abs(2 * light - 1) + 1e-12))
        h = np.zeros_like(mx)
        mask = d != 0
        rm = mask & (mx == r)
        gm = mask & (mx == g) & ~rm
        bm = mask & ~rm & ~gm
        h[rm] = ((g[rm] - b[rm]) / d[rm]) % 6
        h[gm] = (b[gm] - r[gm]) / d[gm] + 2
        h[bm] = (r[bm] - g[bm]) / d[bm] + 4
        h = (h * 60 + hue) % 360
        s = np.clip(s * (1 + saturation / 100.0), 0, 1)
        light = np.clip(light + lightness / 200.0, 0, 1)
        c = (1 - np.abs(2 * light - 1)) * s
        x = c * (1 - np.abs((h / 60) % 2 - 1))
        m = light - c / 2
        zeros = np.zeros_like(h)
        idx = (h // 60).astype(int) % 6
        rr = np.choose(idx, [c, x, zeros, zeros, x, c])
        gg = np.choose(idx, [x, c, c, x, zeros, zeros])
        bb = np.choose(idx, [zeros, zeros, x, c, c, x])
        a[..., 2] = rr + m
        a[..., 1] = gg + m
        a[..., 0] = bb + m
        return from_array(a * 255.0)

    return _apply_region(img, rect, fn)


def rotate(img: QImage, degrees: int) -> QImage:
    t = QTransform()
    t.rotate(degrees)
    return img.transformed(t, Qt.TransformationMode.SmoothTransformation)


def flip(img: QImage, horizontal: bool) -> QImage:
    if hasattr(img, "flipped"):
        return img.flipped(Qt.Orientation.Horizontal if horizontal else Qt.Orientation.Vertical)
    return img.mirrored(horizontal, not horizontal)


def resize(img: QImage, width: int, height: int, smooth: bool = True) -> QImage:
    mode = Qt.TransformationMode.SmoothTransformation if smooth else Qt.TransformationMode.FastTransformation
    return img.scaled(width, height, Qt.AspectRatioMode.IgnoreAspectRatio, mode)


def canvas_size(img: QImage, width: int, height: int, anchor: str = "center", fill: QColor | None = None) -> QImage:
    out = QImage(width, height, QImage.Format.Format_ARGB32)
    out.fill(fill or QColor("white"))
    dx = {"left": 0, "center": (width - img.width()) // 2, "right": width - img.width()}
    dy = {"top": 0, "center": (height - img.height()) // 2, "bottom": height - img.height()}
    ax, ay = ("center", "center")
    if "-" in anchor:
        ay, ax = anchor.split("-", 1)
    elif anchor in dx:
        ax = anchor
    painter = QPainter(out)
    painter.drawImage(QPoint(dx[ax], dy.get(ay, dy["center"])), img)
    painter.end()
    return out


def border(img: QImage, width: int = 4, color: QColor | None = None, inside: bool = False) -> QImage:
    color = color or QColor("black")
    if inside:
        out = img.convertToFormat(QImage.Format.Format_ARGB32)
        painter = QPainter(out)
        pen = QPen(color, width)
        pen.setJoinStyle(Qt.PenJoinStyle.MiterJoin)
        painter.setPen(pen)
        half = width / 2
        painter.drawRect(QRectF(half, half, img.width() - width, img.height() - width))
        painter.end()
        return out
    out = QImage(img.width() + width * 2, img.height() + width * 2, QImage.Format.Format_ARGB32)
    out.fill(color)
    painter = QPainter(out)
    painter.drawImage(QPoint(width, width), img)
    painter.end()
    return out


def drop_shadow(img: QImage, offset: int = 8, radius: int = 6, bg: QColor | None = None) -> QImage:
    pad = offset + radius * 3
    w, h = img.width() + pad, img.height() + pad
    shadow = np.zeros((h, w, 4), dtype=np.float64)
    shadow[offset : offset + img.height(), offset : offset + img.width(), 3] = 150
    for _ in range(3):
        shadow = _box_blur_axis(shadow, radius, 0)
        shadow = _box_blur_axis(shadow, radius, 1)
    out = QImage(w, h, QImage.Format.Format_ARGB32)
    out.fill(bg if bg is not None else QColor(0, 0, 0, 0))
    painter = QPainter(out)
    painter.drawImage(QPoint(0, 0), from_array(shadow))
    painter.drawImage(QPoint(0, 0), img)
    painter.end()
    return out


def watermark(
    img: QImage, text: str, color: QColor | None = None, opacity: float = 0.35, position: str = "bottom-right", size: int = 24
) -> QImage:
    out = img.convertToFormat(QImage.Format.Format_ARGB32)
    painter = QPainter(out)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setOpacity(opacity)
    font = QFont()
    font.setPixelSize(size)
    font.setBold(True)
    painter.setFont(font)
    painter.setPen(color or QColor("white"))
    margin = 12
    if position == "tile":
        metrics = painter.fontMetrics()
        tw = metrics.horizontalAdvance(text) + 60
        th = metrics.height() + 60
        painter.translate(out.width() / 2, out.height() / 2)
        painter.rotate(-30)
        diag = int((out.width() ** 2 + out.height() ** 2) ** 0.5)
        for y in range(-diag, diag, th):
            for x in range(-diag, diag, tw):
                painter.drawText(QPointF(x, y), text)
    else:
        flags = {
            "top-left": Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft,
            "top-right": Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignRight,
            "bottom-left": Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignLeft,
            "bottom-right": Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignRight,
            "center": Qt.AlignmentFlag.AlignCenter,
        }[position]
        painter.drawText(out.rect().adjusted(margin, margin, -margin, -margin), flags, text)
    painter.end()
    return out


def fill_rect(img: QImage, rect: QRect, color: QColor) -> QImage:
    out = img.convertToFormat(QImage.Format.Format_ARGB32)
    painter = QPainter(out)
    painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
    painter.fillRect(rect, color)
    painter.end()
    return out


def crop_to_path(img: QImage, path: QPainterPath) -> QImage:
    rect = path.boundingRect().toAlignedRect().intersected(img.rect())
    out = QImage(rect.size(), QImage.Format.Format_ARGB32)
    out.fill(0)
    painter = QPainter(out)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.translate(-rect.topLeft())
    painter.setClipPath(path)
    painter.drawImage(QPoint(0, 0), img)
    painter.end()
    return out


def flood_fill(img: QImage, x: int, y: int, color: QColor, tolerance: int = 16) -> QImage:
    """스캔라인 방식 영역 채우기."""
    a = to_array(img)
    h, w = a.shape[:2]
    if not (0 <= x < w and 0 <= y < h):
        return img
    target = a[y, x].astype(np.int16)
    diff = np.abs(a.astype(np.int16) - target).max(axis=-1)
    mask = diff <= tolerance
    filled = np.zeros((h, w), dtype=bool)
    stack = [(x, y)]
    while stack:
        sx, sy = stack.pop()
        if filled[sy, sx] or not mask[sy, sx]:
            continue
        row = mask[sy]
        left_part = row[: sx + 1][::-1]
        stop = np.argmin(left_part) if not left_part.all() else left_part.size
        left = sx - stop + 1
        right_part = row[sx:]
        stop = np.argmin(right_part) if not right_part.all() else right_part.size
        right = sx + stop - 1
        filled[sy, left : right + 1] = True
        for ny in (sy - 1, sy + 1):
            if 0 <= ny < h:
                seg = mask[ny, left : right + 1] & ~filled[ny, left : right + 1]
                if seg.any():
                    d = np.diff(np.concatenate(([0], seg.astype(np.int8))))
                    for s in np.nonzero(d == 1)[0]:
                        stack.append((left + int(s), ny))
    a[filled] = [color.blue(), color.green(), color.red(), color.alpha()]
    return from_array(a)
