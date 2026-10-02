"""색상 추출 도구 / 색상 팔레트."""

from __future__ import annotations

from PySide6.QtCore import QPoint, Qt, Signal
from PySide6.QtGui import QColor, QGuiApplication, QKeyEvent, QMouseEvent, QPainter, QPaintEvent
from PySide6.QtWidgets import (
    QColorDialog,
    QComboBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from mikmick.capture.backend import Snapshot
from mikmick.capture.overlay import FrozenOverlay
from mikmick.config import Config
from mikmick.icons import icon

COLOR_FORMATS = [
    ("html", "HTML (#RRGGBB)"),
    ("hex", "HEX (RRGGBB)"),
    ("rgb", "RGB (r, g, b)"),
    ("rgba", "RGBA (r, g, b, a)"),
    ("hsb", "HSB (h, s, b)"),
    ("cpp", "C++ (0x00BBGGRR)"),
    ("delphi", "Delphi ($00BBGGRR)"),
]


def format_color(c: QColor, fmt: str) -> str:
    r, g, b = c.red(), c.green(), c.blue()
    if fmt == "hex":
        return f"{r:02X}{g:02X}{b:02X}"
    if fmt == "rgb":
        return f"rgb({r}, {g}, {b})"
    if fmt == "rgba":
        return f"rgba({r}, {g}, {b}, {c.alphaF():.2f})"
    if fmt == "hsb":
        h, s, v, _a = c.getHsv()
        return f"hsb({max(0, h)}, {round(s / 2.55)}%, {round(v / 2.55)}%)"
    if fmt == "cpp":
        return f"0x00{b:02X}{g:02X}{r:02X}"
    if fmt == "delphi":
        return f"$00{b:02X}{g:02X}{r:02X}"
    return f"#{r:02X}{g:02X}{b:02X}"


class ColorPickOverlay(FrozenOverlay):
    picked = Signal(QColor)

    def __init__(self, snapshot: Snapshot, zoom: int = 8):
        super().__init__(snapshot, True, zoom)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        self.mouse = event.position().toPoint()
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            c = self.color_at(event.position().toPoint())
            self.hide()
            self.picked.emit(c)
            self.close()
        else:
            self.cancel()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter, Qt.Key.Key_Space):
            c = self.color_at(self.mouse)
            self.hide()
            self.picked.emit(c)
            self.close()
            return
        super().keyPressEvent(event)

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        self.paint_background(p)
        self.draw_magnifier(p)
        self.draw_hint(p, "클릭하여 색상을 추출하세요.  방향키: 1px 이동   ESC: 취소")
        p.end()


class ColorSwatchLabel(QLabel):
    def __init__(self):
        super().__init__()
        self.color = QColor("white")
        self.setFixedSize(64, 64)

    def set_color(self, c: QColor) -> None:
        self.color = QColor(c)
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        p.fillRect(self.rect(), self.color)
        p.setPen(QColor("#888"))
        p.drawRect(self.rect().adjusted(0, 0, -1, -1))
        p.end()


class ColorToolWindow(QWidget):
    """색상 추출 결과 + 팔레트(편집) 창."""

    pick_again = Signal()

    def __init__(self, config: Config, palette_mode: bool = False):
        super().__init__(None, Qt.WindowType.Window)
        self.config = config
        self.setWindowTitle("색상 팔레트" if palette_mode else "색상 추출 도구")
        self.setWindowIcon(icon("tool_palette" if palette_mode else "tool_color_picker"))
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        root = QVBoxLayout(self)

        self.dialog = QColorDialog(self)
        self.dialog.setOptions(
            QColorDialog.ColorDialogOption.NoButtons
            | QColorDialog.ColorDialogOption.DontUseNativeDialog
            | QColorDialog.ColorDialogOption.ShowAlphaChannel
        )
        self.dialog.setWindowFlags(Qt.WindowType.Widget)
        for i, c in enumerate(config.get("palette", "custom", [])[:16]):
            QColorDialog.setCustomColor(i, QColor(c))
        root.addWidget(self.dialog)

        row = QHBoxLayout()
        self.swatch = ColorSwatchLabel()
        row.addWidget(self.swatch)
        col = QVBoxLayout()
        self.fmt = QComboBox()
        for key, label in COLOR_FORMATS:
            self.fmt.addItem(label, key)
        self.fmt.setCurrentIndex(max(0, self.fmt.findData(config.get("palette", "format", "html"))))
        col.addWidget(self.fmt)
        self.code = QLineEdit()
        self.code.setReadOnly(True)
        col.addWidget(self.code)
        row.addLayout(col, 1)
        root.addLayout(row)

        buttons = QHBoxLayout()
        b_pick = QPushButton(icon("eyedropper"), "화면에서 추출")
        b_pick.clicked.connect(self.pick_again)
        b_copy = QPushButton(icon("copy"), "코드 복사")
        b_copy.clicked.connect(self.copy_code)
        b_save = QPushButton("팔레트에 저장")
        b_save.clicked.connect(self.save_custom)
        b_close = QPushButton("닫기")
        b_close.clicked.connect(self.close)
        for b in (b_pick, b_copy, b_save):
            buttons.addWidget(b)
        buttons.addStretch(1)
        buttons.addWidget(b_close)
        root.addLayout(buttons)

        self.dialog.currentColorChanged.connect(self._update)
        self.fmt.currentIndexChanged.connect(self._fmt_changed)
        self._update(self.dialog.currentColor())

    def set_color(self, c: QColor) -> None:
        self.dialog.setCurrentColor(c)
        self._update(c)

    def _fmt_changed(self) -> None:
        self.config.set("palette", "format", self.fmt.currentData())
        self.config.save()
        self._update(self.dialog.currentColor())

    def _update(self, c: QColor) -> None:
        self.swatch.set_color(c)
        self.code.setText(format_color(c, self.fmt.currentData()))

    def copy_code(self) -> None:
        QGuiApplication.clipboard().setText(self.code.text())

    def save_custom(self) -> None:
        name = self.dialog.currentColor().name()
        custom = [x for x in self.config.get("palette", "custom", []) if x != name]
        custom.insert(0, name)
        self.config.set("palette", "custom", custom[:16])
        self.config.save()
        for i, c in enumerate(custom[:16]):
            QColorDialog.setCustomColor(i, QColor(c))

    def bring_to_front(self, pos: QPoint | None = None) -> None:
        if pos is not None:
            self.move(pos)
        self.show()
        self.raise_()
        self.activateWindow()
