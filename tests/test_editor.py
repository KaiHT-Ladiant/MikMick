"""에디터 문서 / 캔버스 동작 (offscreen)."""

from PySide6.QtCore import QPointF, QRect
from PySide6.QtGui import QColor, QImage

from mikmick.editor import effects
from mikmick.editor.canvas import Document
from mikmick.editor.items import ShapeItem


def _doc(qapp):
    img = QImage(100, 80, QImage.Format.Format_ARGB32)
    img.fill(QColor("white"))
    return Document(img)


def test_add_item_undo_redo(qapp):
    doc = _doc(qapp)
    item = ShapeItem("rect", QPointF(10, 10), QPointF(50, 40), QColor("red"), 3)
    doc.add_item(item)
    assert item in doc.objects()
    doc.undo.undo()
    assert item not in doc.objects()
    doc.undo.redo()
    assert item in doc.objects()


def test_flatten_includes_objects(qapp):
    doc = _doc(qapp)
    doc.add_item(ShapeItem("rect", QPointF(10, 10), QPointF(50, 40), QColor("red"), 4, QColor("red")))
    flat = doc.flatten()
    assert flat.pixelColor(30, 25).red() == 255
    assert flat.pixelColor(30, 25).green() == 0
    assert flat.pixelColor(90, 70) == QColor("white")


def test_crop_selection_merges_and_is_undoable(qapp):
    doc = _doc(qapp)
    doc.add_item(ShapeItem("ellipse", QPointF(0, 0), QPointF(20, 20), QColor("blue"), 2))
    doc.set_selection(QRect(10, 10, 30, 20))
    assert doc.crop_selection()
    assert (doc.size.width(), doc.size.height()) == (30, 20)
    assert doc.objects() == []
    doc.undo.undo()
    assert (doc.size.width(), doc.size.height()) == (100, 80)
    assert len(doc.objects()) == 1


def test_modified_flag(qapp):
    doc = _doc(qapp)
    assert doc.modified  # 아직 저장되지 않은 새 이미지
    doc.path = "/tmp/x.png"
    doc.undo.setClean()
    assert not doc.modified
    doc.apply_raster(lambda im: effects.flip(im, True), "flip")
    assert doc.modified
