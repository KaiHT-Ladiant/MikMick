"""에디터 윈도우 통합 동작 (offscreen)."""

from PySide6.QtCore import QPoint, Qt
from PySide6.QtGui import QColor, QImage
from PySide6.QtTest import QTest

from mikmick.editor import effects


def _window(qapp, config):
    from mikmick.app import Controller

    ctl = Controller(qapp, config)
    ed = ctl.editor
    ed.resize(1200, 800)
    ed.show()
    return ctl, ed


def _image():
    img = QImage(300, 200, QImage.Format.Format_ARGB32)
    img.fill(QColor(10, 120, 200))
    return img


def _drag(view, a: QPoint, b: QPoint):
    vp = view.viewport()
    QTest.mousePress(vp, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, a)
    for i in range(1, 6):
        QTest.mouseMove(vp, a + (b - a) * (i / 5))
    QTest.mouseRelease(vp, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, b)


def test_draw_select_effect_crop_save(qapp, config, tmp_path):
    config.set("editor", "no_save_prompt", True)
    ctl, ed = _window(qapp, config)
    doc = ed.add_image(_image(), title="t")
    view = ed.view()
    view.set_zoom(1.0)
    qapp.processEvents()

    ed.set_tool("shape")
    _drag(view, view.mapFromScene(20, 20), view.mapFromScene(120, 90))
    assert len(doc.objects()) == 1

    ed.set_tool("select")
    _drag(view, view.mapFromScene(10, 10), view.mapFromScene(160, 110))
    assert doc.selection is not None and doc.selection.width() >= 140

    ed._fx("무채화", lambda img, r: effects.grayscale(img, r))
    c = doc.image.pixelColor(150, 100)
    assert c.red() == c.green() == c.blue()
    assert doc.objects() == []

    ed.crop()
    assert doc.size.width() < 300

    path = tmp_path / "out.png"
    assert ed._save_to(doc, str(path))
    assert path.exists() and not doc.modified

    ed.undo()
    assert doc.size.width() == 300

    assert ed.open_path(str(path))
    assert ed.tabs.count() == 2
    assert ed.close_all()
    ctl.hotkeys.stop()


def test_text_and_stamp_tools(qapp, config):
    config.set("editor", "no_save_prompt", True)
    ctl, ed = _window(qapp, config)
    doc = ed.add_image(_image())
    view = ed.view()
    view.set_zoom(1.0)
    qapp.processEvents()
    ed._set_stamp("number")
    for x in (30, 60, 90):
        QTest.mouseClick(view.viewport(), Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, view.mapFromScene(x, 40))
    numbers = sorted(it.number for it in doc.objects())
    assert numbers == [1, 2, 3]
    ed.set_tool("move")
    doc.scene.clearSelection()
    doc.objects()[0].setSelected(True)
    ed.delete()
    assert len(doc.objects()) == 2
    ed.undo()
    assert len(doc.objects()) == 3
    assert ed.close_all()
    ctl.hotkeys.stop()
