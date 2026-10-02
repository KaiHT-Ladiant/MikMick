"""README 용 스크린샷을 오프스크린으로 생성한다.

    QT_QPA_PLATFORM=offscreen python scripts/make_screenshots.py
"""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from PySide6.QtCore import QPoint, QPointF, QRect  # noqa: E402
from PySide6.QtGui import QColor, QFont, QImage, QLinearGradient, QPainter, QPixmap  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

OUT = Path(__file__).resolve().parents[1] / "docs" / "screenshots"


def sample_image(w: int = 900, h: int = 520) -> QImage:
    img = QImage(w, h, QImage.Format.Format_ARGB32)
    p = QPainter(img)
    grad = QLinearGradient(0, 0, w, h)
    grad.setColorAt(0, QColor("#2c3e50"))
    grad.setColorAt(1, QColor("#4ca1af"))
    p.fillRect(img.rect(), grad)
    p.fillRect(QRect(60, 50, w - 120, h - 100), QColor("#fdfdfd"))
    p.fillRect(QRect(60, 50, w - 120, 34), QColor("#e8e8e8"))
    p.setPen(QColor("#333"))
    f = QFont()
    f.setPixelSize(22)
    f.setBold(True)
    p.setFont(f)
    p.drawText(QRect(90, 110, 600, 40), 0, "MikMick - Linux Screen Capture")
    f.setBold(False)
    f.setPixelSize(15)
    p.setFont(f)
    for i in range(8):
        p.fillRect(QRect(90, 170 + i * 30, 300 + (i * 37) % 260, 12), QColor("#c9d3dd"))
    p.fillRect(QRect(560, 170, 220, 220), QColor("#ffd166"))
    p.end()
    return img


def save(widget, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    QApplication.processEvents()
    pm = widget.grab()
    pm.save(str(OUT / f"{name}.png"))
    print("saved", name, pm.size().toTuple())


def main() -> None:
    tmp = tempfile.mkdtemp()
    os.environ["XDG_CONFIG_HOME"] = tmp
    app = QApplication(sys.argv[:1])
    app.setFont(QFont("Malgun Gothic" if sys.platform == "win32" else "Noto Sans CJK KR", 9))

    from mikmick.app import Controller
    from mikmick.config import Config

    config = Config()
    ctl = Controller(app, config)
    ed = ctl.editor
    ed.resize(1100, 700)
    ed.show()
    ed.show_backstage("start")
    save(ed, "start")
    ed.show_backstage("new")
    save(ed, "new")
    ed.show_backstage("info")
    save(ed, "info")
    ed.hide_backstage()
    save(ed, "editor_empty")

    from mikmick.editor.items import ShapeItem, StampItem, StrokeItem, TextItem

    doc = ed.add_image(sample_image(), title="캡처 이미지")
    doc.add_item(ShapeItem("rect", QPointF(550, 160), QPointF(790, 400), QColor("#e81123"), 4))
    doc.add_item(ShapeItem("arrow", QPointF(380, 420), QPointF(540, 330), QColor("#e81123"), 5))
    s1 = StampItem("number", 1, QColor("#e81123"), 30)
    s1.setPos(540, 150)
    doc.add_item(s1)
    s2 = StampItem("number", 2, QColor("#0078d7"), 30)
    s2.setPos(80, 115)
    doc.add_item(s2)
    t = TextItem("여기를 확인하세요!", QFont("Sans", 18), QColor("#e81123"))
    t.setPos(250, 425)
    doc.add_item(t)
    stroke = StrokeItem(QPointF(90, 300), QColor("#ffe600"), 18, highlighter=True)
    for x in range(95, 400, 5):
        stroke.add_point(QPointF(x, 300))
    doc.add_item(stroke)
    view = ed.view()
    view.set_zoom(1.0)
    ed.set_tool("shape")
    save(ed, "editor")

    from mikmick.options import OptionsDialog

    dlg = OptionsDialog(config)
    dlg.show()
    for i, name in enumerate(["general", "editor", "capture", "filename", "autosave", "image", "ftp", "hotkeys"]):
        dlg.nav.setCurrentRow(i)
        save(dlg, f"options_{name}")
    dlg.close()

    from mikmick.capture.backend import Snapshot
    from mikmick.capture.overlay import CaptureOverlay

    bg = QPixmap.fromImage(sample_image(1280, 720))
    snap = Snapshot(bg, QRect(0, 0, 1280, 720))
    ov = CaptureOverlay(snap, "region", True, 6)
    ov.setGeometry(snap.geometry)
    ov.show()
    ov.current = QRect(520, 140, 300, 260)
    ov.mouse = QPoint(820, 400)
    ov.dragging = True
    ov.origin = QPoint(520, 140)
    save(ov, "capture_region")
    ov.dragging = False
    ov.close()

    from mikmick.tools.colors import ColorToolWindow

    cw = ColorToolWindow(config, True)
    cw.set_color(QColor("#4ca1af"))
    cw.show()
    save(cw, "color_palette")
    cw.close()

    for v in ed.views():
        v.doc.path = "/tmp/x.png"
        v.doc.undo.setClean()
    config.set("editor", "no_save_prompt", True)
    ed.close_all()


if __name__ == "__main__":
    main()
