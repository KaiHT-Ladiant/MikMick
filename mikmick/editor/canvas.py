"""편집 문서(Document)와 캔버스 뷰(CanvasView)."""

from __future__ import annotations

import os
from collections.abc import Callable
from dataclasses import dataclass, field

from PySide6.QtCore import QObject, QPoint, QPointF, QRect, QRectF, Qt, Signal
from PySide6.QtGui import (
    QBrush,
    QColor,
    QFont,
    QImage,
    QKeyEvent,
    QMouseEvent,
    QPainter,
    QPen,
    QPixmap,
    QUndoCommand,
    QUndoStack,
    QWheelEvent,
)
from PySide6.QtWidgets import (
    QGraphicsItem,
    QGraphicsPixmapItem,
    QGraphicsRectItem,
    QGraphicsScene,
    QGraphicsView,
)

from mikmick.editor import effects
from mikmick.editor.items import PixmapObject, ShapeItem, StampItem, StrokeItem, TextItem


@dataclass
class ToolState:
    tool: str = "move"
    color1: QColor = field(default_factory=lambda: QColor("#e81123"))
    color2: QColor = field(default_factory=lambda: QColor("#ffffff"))
    width: int = 3
    draw_kind: str = "pen"  # pen | highlighter | eraser
    shape_kind: str = "rect"
    shape_fill: bool = False
    stamp_kind: str = "number"  # number | cursor
    stamp_size: int = 28
    font: QFont = field(default_factory=lambda: QFont("Sans", 16))
    fill_tolerance: int = 16


# --- Undo 명령 ---------------------------------------------------------------


class ImageCommand(QUndoCommand):
    def __init__(self, doc: Document, before: QImage, after: QImage, text: str):
        super().__init__(text)
        self.doc = doc
        self.before = before
        self.after = after

    def redo(self) -> None:
        self.doc._set_background(self.after)

    def undo(self) -> None:
        self.doc._set_background(self.before)


class AddItemCommand(QUndoCommand):
    def __init__(self, doc: Document, item: QGraphicsItem, text: str = "개체 추가", already_added: bool = False):
        super().__init__(text)
        self.doc = doc
        self.item = item
        self.skip_first = already_added

    def redo(self) -> None:
        if self.skip_first:
            self.skip_first = False
            return
        if self.item.scene() is None:
            self.doc.scene.addItem(self.item)

    def undo(self) -> None:
        if self.item.scene() is not None:
            self.doc.scene.removeItem(self.item)


class RemoveItemsCommand(QUndoCommand):
    def __init__(self, doc: Document, items: list[QGraphicsItem], text: str = "개체 삭제"):
        super().__init__(text)
        self.doc = doc
        self.items = items

    def redo(self) -> None:
        for it in self.items:
            if it.scene() is not None:
                self.doc.scene.removeItem(it)

    def undo(self) -> None:
        for it in self.items:
            if it.scene() is None:
                self.doc.scene.addItem(it)


class MoveItemsCommand(QUndoCommand):
    def __init__(self, items: list[tuple[QGraphicsItem, QPointF, QPointF]]):
        super().__init__("개체 이동")
        self.items = items

    def redo(self) -> None:
        for it, _old, new in self.items:
            it.setPos(new)

    def undo(self) -> None:
        for it, old, _new in self.items:
            it.setPos(old)


# --- 문서 --------------------------------------------------------------------


class Document(QObject):
    changed = Signal()
    selection_changed = Signal()

    def __init__(self, image: QImage, path: str | None = None, title: str | None = None):
        super().__init__()
        self.path = path
        self.title = title or (os.path.basename(path) if path else "제목 없음")
        self.scene = QGraphicsScene()
        self.undo = QUndoStack(self)
        self.bg_item = QGraphicsPixmapItem()
        self.bg_item.setZValue(-1000)
        self.bg_item.setTransformationMode(Qt.TransformationMode.FastTransformation)
        self.scene.addItem(self.bg_item)
        self._image = QImage()
        self.selection: QRect | None = None
        self.selection_item = QGraphicsRectItem()
        pen = QPen(QColor(0, 0, 0), 0, Qt.PenStyle.DashLine)
        pen.setCosmetic(True)
        self.selection_item.setPen(pen)
        self.selection_item.setBrush(QBrush(QColor(47, 140, 255, 30)))
        self.selection_item.setZValue(10_000)
        self.selection_item.hide()
        self.scene.addItem(self.selection_item)
        self._set_background(image.convertToFormat(QImage.Format.Format_ARGB32))
        self.undo.indexChanged.connect(self._on_undo_index)

    def _on_undo_index(self, _index: int) -> None:
        self.changed.emit()

    # 기본 정보 ------------------------------------------------------------
    @property
    def image(self) -> QImage:
        return self._image

    @property
    def size(self):
        return self._image.size()

    @property
    def modified(self) -> bool:
        return self.path is None or not self.undo.isClean()

    def objects(self) -> list[QGraphicsItem]:
        return [it for it in self.scene.items() if it is not self.bg_item and it is not self.selection_item and it.parentItem() is None]

    def _set_background(self, image: QImage) -> None:
        self._image = image
        self.bg_item.setPixmap(QPixmap.fromImage(image))
        self.scene.setSceneRect(QRectF(image.rect()))
        if self.selection is not None and not image.rect().contains(self.selection):
            self.set_selection(None)
        self.changed.emit()

    # 선택 영역 ------------------------------------------------------------
    def set_selection(self, rect: QRect | None) -> None:
        if rect is not None:
            rect = rect.normalized().intersected(self._image.rect())
            if rect.width() < 2 or rect.height() < 2:
                rect = None
        self.selection = rect
        if rect is None:
            self.selection_item.hide()
        else:
            self.selection_item.setRect(QRectF(rect))
            self.selection_item.show()
        self.selection_changed.emit()

    # 렌더링 ---------------------------------------------------------------
    def flatten(self) -> QImage:
        out = QImage(self._image.size(), QImage.Format.Format_ARGB32)
        out.fill(0)
        selected = self.scene.selectedItems()
        for it in selected:
            it.setSelected(False)
        sel_visible = self.selection_item.isVisible()
        self.selection_item.hide()
        focus = self.scene.focusItem()
        if focus is not None:
            focus.clearFocus()
        painter = QPainter(out)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
        self.scene.render(painter, QRectF(out.rect()), QRectF(out.rect()))
        painter.end()
        self.selection_item.setVisible(sel_visible)
        for it in selected:
            it.setSelected(True)
        return out

    # 편집 -----------------------------------------------------------------
    def apply_raster(self, fn: Callable[[QImage], QImage], text: str) -> None:
        """개체를 이미지에 병합한 뒤 fn 을 적용한다 (하나의 실행 취소 단위)."""
        objs = self.objects()
        before = self._image
        base = self.flatten() if objs else self._image
        after = fn(base)
        if after is None or after.isNull():
            return
        self.undo.beginMacro(text)
        if objs:
            self.undo.push(RemoveItemsCommand(self, objs, "개체 병합"))
        self.undo.push(ImageCommand(self, before, after, text))
        self.undo.endMacro()

    def merge_objects(self) -> None:
        if self.objects():
            self.apply_raster(lambda img: img, "개체 병합")

    def add_item(self, item: QGraphicsItem, text: str = "개체 추가") -> None:
        self.undo.push(AddItemCommand(self, item, text))

    def remove_items(self, items: list[QGraphicsItem]) -> None:
        if items:
            self.undo.push(RemoveItemsCommand(self, items))

    def crop_selection(self) -> bool:
        if self.selection is None:
            return False
        rect = QRect(self.selection)
        self.set_selection(None)
        self.apply_raster(lambda img: img.copy(rect), "자르기")
        return True

    def selection_image(self) -> QImage:
        flat = self.flatten()
        if self.selection is not None:
            return flat.copy(self.selection)
        return flat


# --- 캔버스 뷰 ---------------------------------------------------------------


class CanvasView(QGraphicsView):
    cursor_moved = Signal(QPoint)
    zoom_changed = Signal(float)
    color_picked = Signal(QColor, bool)  # 색상, 색1 여부
    tool_finished = Signal()

    def __init__(self, doc: Document, state: ToolState, stamp_counter: Callable[[], int], bg: QBrush, centered: bool):
        super().__init__(doc.scene)
        self.doc = doc
        self.state = state
        self.next_stamp = stamp_counter
        self.zoom = 1.0
        self._temp: QGraphicsItem | None = None
        self._origin: QPointF | None = None
        self._move_start: dict[QGraphicsItem, QPointF] = {}
        self._panning: QPoint | None = None
        self.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.setBackgroundBrush(bg)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter if centered else Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.setMouseTracking(True)
        self.setFrameShape(QGraphicsView.Shape.NoFrame)
        self.apply_tool()

    # 도구 -----------------------------------------------------------------
    def apply_tool(self) -> None:
        tool = self.state.tool
        self.setDragMode(QGraphicsView.DragMode.RubberBandDrag if tool == "move" else QGraphicsView.DragMode.NoDrag)
        interactive = tool == "move"
        for it in self.doc.objects():
            it.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, interactive)
            it.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, interactive or tool == "text")
        if not interactive:
            self.doc.scene.clearSelection()
        cursors = {
            "move": Qt.CursorShape.ArrowCursor,
            "select": Qt.CursorShape.CrossCursor,
            "draw": Qt.CursorShape.CrossCursor,
            "fill": Qt.CursorShape.PointingHandCursor,
            "text": Qt.CursorShape.IBeamCursor,
            "stamp": Qt.CursorShape.CrossCursor,
            "shape": Qt.CursorShape.CrossCursor,
            "eyedropper": Qt.CursorShape.CrossCursor,
        }
        self.viewport().setCursor(cursors.get(tool, Qt.CursorShape.ArrowCursor))

    def set_zoom(self, z: float) -> None:
        z = max(0.05, min(32.0, z))
        self.zoom = z
        self.resetTransform()
        self.scale(z, z)
        self.zoom_changed.emit(z)

    def fit(self) -> None:
        r = self.doc.scene.sceneRect()
        vp = self.viewport().size()
        if r.isEmpty() or vp.isEmpty():
            return
        z = min((vp.width() - 20) / r.width(), (vp.height() - 20) / r.height())
        self.set_zoom(min(1.0, z))

    def _scene_point(self, event: QMouseEvent) -> QPointF:
        return self.mapToScene(event.position().toPoint())

    def _clamped(self, p: QPointF) -> QPointF:
        r = self.doc.scene.sceneRect()
        return QPointF(min(max(p.x(), r.left()), r.right()), min(max(p.y(), r.top()), r.bottom()))

    # 마우스 ---------------------------------------------------------------
    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.MiddleButton:
            self._panning = event.position().toPoint()
            self.viewport().setCursor(Qt.CursorShape.ClosedHandCursor)
            return
        tool = self.state.tool
        p = self._scene_point(event)
        if tool == "move":
            super().mousePressEvent(event)
            self._move_start = {it: it.pos() for it in self.doc.scene.selectedItems()}
            return
        if tool == "text":
            item = self.doc.scene.itemAt(p, self.transform())
            if isinstance(item, TextItem):
                super().mousePressEvent(event)
                return
            focus = self.doc.scene.focusItem()
            if isinstance(focus, TextItem):
                focus.clearFocus()
                return
        if event.button() not in (Qt.MouseButton.LeftButton, Qt.MouseButton.RightButton):
            return
        primary = event.button() == Qt.MouseButton.LeftButton
        color = self.state.color1 if primary else self.state.color2
        if tool == "eyedropper":
            img = self.doc.flatten()
            ip = p.toPoint()
            if img.rect().contains(ip):
                self.color_picked.emit(img.pixelColor(ip), primary)
            return
        if tool == "fill":
            ip = p.toPoint()
            if self.doc.image.rect().contains(ip):
                tol = self.state.fill_tolerance
                self.doc.apply_raster(lambda img: effects.flood_fill(img, ip.x(), ip.y(), color, tol), "채우기")
            return
        if tool == "text":
            item = TextItem("텍스트", self.state.font, color)
            item.setPos(p)
            self.doc.add_item(item, "텍스트")
            item.start_editing()
            return
        if tool == "stamp":
            if self.state.stamp_kind == "cursor":
                item = StampItem("cursor", 0, color, self.state.stamp_size)
            else:
                item = StampItem("number", self.next_stamp(), color, self.state.stamp_size)
            item.setPos(p)
            self.doc.add_item(item, "스탬프")
            return
        self._origin = p
        if tool == "select":
            if self.doc.objects() and self.window() is not None and getattr(self.window(), "auto_merge_on_select", False):
                self.doc.merge_objects()
            self.doc.set_selection(None)
        elif tool == "draw":
            kind = self.state.draw_kind
            if kind == "highlighter":
                c = QColor(color)
                c.setAlpha(255)
                item = StrokeItem(p, c, max(8, self.state.width * 4), highlighter=True)
            elif kind == "eraser":
                item = StrokeItem(p, self.state.color2, max(6, self.state.width * 3))
            else:
                item = StrokeItem(p, color, self.state.width)
            self.doc.scene.addItem(item)
            self._temp = item
        elif tool == "shape":
            fill = self.state.color2 if self.state.shape_fill else None
            if not primary and fill is not None:
                fill = self.state.color1
            item = ShapeItem(self.state.shape_kind, p, p, color, self.state.width, fill)
            self.doc.scene.addItem(item)
            self._temp = item

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self._panning is not None:
            delta = event.position().toPoint() - self._panning
            self._panning = event.position().toPoint()
            self.horizontalScrollBar().setValue(self.horizontalScrollBar().value() - delta.x())
            self.verticalScrollBar().setValue(self.verticalScrollBar().value() - delta.y())
            return
        p = self._scene_point(event)
        self.cursor_moved.emit(p.toPoint())
        tool = self.state.tool
        if tool in ("move", "text"):
            super().mouseMoveEvent(event)
            return
        if self._origin is None:
            return
        if tool == "select":
            self.doc.set_selection(QRectF(self._origin, self._clamped(p)).toAlignedRect())
        elif isinstance(self._temp, StrokeItem):
            self._temp.add_point(p)
        elif isinstance(self._temp, ShapeItem):
            end = p
            if event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                end = self._constrain(self._origin, p, self._temp.kind)
            self._temp.set_end(end)

    @staticmethod
    def _constrain(o: QPointF, p: QPointF, kind: str) -> QPointF:
        dx, dy = p.x() - o.x(), p.y() - o.y()
        if kind in ("line", "arrow"):
            if abs(dx) > 2 * abs(dy):
                return QPointF(p.x(), o.y())
            if abs(dy) > 2 * abs(dx):
                return QPointF(o.x(), p.y())
        s = max(abs(dx), abs(dy))
        return QPointF(o.x() + (s if dx >= 0 else -s), o.y() + (s if dy >= 0 else -s))

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if self._panning is not None and event.button() == Qt.MouseButton.MiddleButton:
            self._panning = None
            self.apply_tool()
            return
        tool = self.state.tool
        if tool in ("move", "text"):
            super().mouseReleaseEvent(event)
            moved = [(it, old, it.pos()) for it, old in self._move_start.items() if it.pos() != old]
            self._move_start = {}
            if moved:
                self.doc.undo.push(MoveItemsCommand(moved))
            return
        if self._temp is not None:
            item = self._temp
            self._temp = None
            r = item.boundingRect()
            if isinstance(item, ShapeItem) and QRectF(item.p1, item.p2).normalized().width() < 2 and QRectF(item.p1, item.p2).normalized().height() < 2:
                self.doc.scene.removeItem(item)
            elif r.isEmpty():
                self.doc.scene.removeItem(item)
            else:
                name = "그리기" if isinstance(item, StrokeItem) else "도형"
                self.doc.undo.push(AddItemCommand(self.doc, item, name, already_added=True))
        self._origin = None

    def wheelEvent(self, event: QWheelEvent) -> None:
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            factor = 1.25 if event.angleDelta().y() > 0 else 0.8
            self.set_zoom(self.zoom * factor)
            return
        super().wheelEvent(event)

    def keyPressEvent(self, event: QKeyEvent) -> None:
        focus = self.doc.scene.focusItem()
        if isinstance(focus, TextItem) and focus.textInteractionFlags() != Qt.TextInteractionFlag.NoTextInteraction:
            if event.key() == Qt.Key.Key_Escape:
                focus.clearFocus()
                return
            super().keyPressEvent(event)
            return
        if event.key() == Qt.Key.Key_Escape:
            self.doc.set_selection(None)
            self.doc.scene.clearSelection()
            return
        selected = self.doc.scene.selectedItems()
        if selected and event.key() in (Qt.Key.Key_Left, Qt.Key.Key_Right, Qt.Key.Key_Up, Qt.Key.Key_Down):
            step = 10 if event.modifiers() & Qt.KeyboardModifier.ShiftModifier else 1
            d = {
                Qt.Key.Key_Left: QPointF(-step, 0),
                Qt.Key.Key_Right: QPointF(step, 0),
                Qt.Key.Key_Up: QPointF(0, -step),
                Qt.Key.Key_Down: QPointF(0, step),
            }[event.key()]
            self.doc.undo.push(MoveItemsCommand([(it, it.pos(), it.pos() + d) for it in selected]))
            return
        super().keyPressEvent(event)

    def paste_pixmap(self, pixmap: QPixmap) -> None:
        item = PixmapObject(pixmap)
        tl = self.mapToScene(QPoint(10, 10))
        item.setPos(self._clamped(tl))
        self.doc.add_item(item, "붙여넣기")
        self.state.tool = "move"
        self.apply_tool()
        self.doc.scene.clearSelection()
        item.setSelected(True)
        self.tool_finished.emit()
