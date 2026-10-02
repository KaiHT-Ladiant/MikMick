"""효과 파라미터 / 크기 조절 대화상자."""

from __future__ import annotations

from typing import Any

from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QCheckBox,
    QColorDialog,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QHBoxLayout,
    QLineEdit,
    QPushButton,
    QSlider,
    QSpinBox,
    QWidget,
)


class ColorButton(QPushButton):
    def __init__(self, color: QColor):
        super().__init__()
        self.color = QColor(color)
        self.setFixedSize(60, 24)
        self.clicked.connect(self._pick)
        self._refresh()

    def _pick(self) -> None:
        c = QColorDialog.getColor(self.color, self, "색 선택", QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if c.isValid():
            self.color = c
            self._refresh()

    def _refresh(self) -> None:
        self.setStyleSheet(f"background: {self.color.name(QColor.NameFormat.HexArgb)}; border: 1px solid #888;")


class ParamDialog(QDialog):
    """필드 정의 목록으로 간단한 입력 대화상자를 만든다.

    field: (key, label, kind, default, extra)
      kind = int(extra=(min,max)) | float(extra=(min,max)) | choice(extra=[(value,label)]) | text | color | bool
    """

    def __init__(self, title: str, fields: list[tuple[str, str, str, Any, Any]], parent: QWidget | None = None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setMinimumWidth(340)
        form = QFormLayout(self)
        self.widgets: dict[str, tuple[str, QWidget]] = {}
        for key, label, kind, default, extra in fields:
            if kind == "int":
                row = QWidget()
                lay = QHBoxLayout(row)
                lay.setContentsMargins(0, 0, 0, 0)
                slider = QSlider(Qt.Orientation.Horizontal)
                slider.setRange(*extra)
                spin = QSpinBox()
                spin.setRange(*extra)
                slider.valueChanged.connect(spin.setValue)
                spin.valueChanged.connect(slider.setValue)
                spin.setValue(default)
                lay.addWidget(slider, 1)
                lay.addWidget(spin)
                form.addRow(label, row)
                self.widgets[key] = (kind, spin)
            elif kind == "float":
                spin = QDoubleSpinBox()
                spin.setRange(*extra)
                spin.setSingleStep(0.05)
                spin.setValue(default)
                form.addRow(label, spin)
                self.widgets[key] = (kind, spin)
            elif kind == "choice":
                combo = QComboBox()
                for value, text in extra:
                    combo.addItem(text, value)
                combo.setCurrentIndex(max(0, combo.findData(default)))
                form.addRow(label, combo)
                self.widgets[key] = (kind, combo)
            elif kind == "text":
                edit = QLineEdit(default)
                form.addRow(label, edit)
                self.widgets[key] = (kind, edit)
            elif kind == "color":
                btn = ColorButton(default)
                form.addRow(label, btn)
                self.widgets[key] = (kind, btn)
            elif kind == "bool":
                chk = QCheckBox()
                chk.setChecked(bool(default))
                form.addRow(label, chk)
                self.widgets[key] = (kind, chk)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText("확인")
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText("취소")
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def values(self) -> dict[str, Any]:
        out: dict[str, Any] = {}
        for key, (kind, w) in self.widgets.items():
            if kind in ("int", "float"):
                out[key] = w.value()
            elif kind == "choice":
                out[key] = w.currentData()
            elif kind == "text":
                out[key] = w.text()
            elif kind == "color":
                out[key] = QColor(w.color)
            elif kind == "bool":
                out[key] = w.isChecked()
        return out


def ask(title: str, fields: list[tuple[str, str, str, Any, Any]], parent: QWidget | None = None) -> dict[str, Any] | None:
    dlg = ParamDialog(title, fields, parent)
    if dlg.exec() == QDialog.DialogCode.Accepted:
        return dlg.values()
    return None


class ResizeDialog(QDialog):
    def __init__(self, size: QSize, title: str = "이미지 크기 조절", parent: QWidget | None = None, with_anchor: bool = False):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.orig = size
        form = QFormLayout(self)
        self.mode = QComboBox()
        self.mode.addItem("픽셀", "px")
        self.mode.addItem("퍼센트", "pct")
        form.addRow("단위", self.mode)
        self.w = QSpinBox()
        self.w.setRange(1, 50000)
        self.h = QSpinBox()
        self.h.setRange(1, 50000)
        form.addRow("가로 (Width)", self.w)
        form.addRow("세로 (Height)", self.h)
        self.keep = QCheckBox("가로 세로 비율 유지")
        self.keep.setChecked(not with_anchor)
        form.addRow("", self.keep)
        self.anchor: QComboBox | None = None
        if with_anchor:
            self.anchor = QComboBox()
            for value, text in (
                ("center", "가운데"),
                ("top-left", "왼쪽 위"),
                ("top-right", "오른쪽 위"),
                ("bottom-left", "왼쪽 아래"),
                ("bottom-right", "오른쪽 아래"),
            ):
                self.anchor.addItem(text, value)
            form.addRow("기준 위치", self.anchor)
            self.fill = ColorButton(QColor("white"))
            form.addRow("배경색", self.fill)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText("확인")
        buttons.button(QDialogButtonBox.StandardButton.Cancel).setText("취소")
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)
        self._busy = False
        self._set_px()
        self.mode.currentIndexChanged.connect(self._mode_changed)
        self.w.valueChanged.connect(lambda _v: self._linked(True))
        self.h.valueChanged.connect(lambda _v: self._linked(False))

    def _set_px(self) -> None:
        self._busy = True
        self.w.setValue(self.orig.width())
        self.h.setValue(self.orig.height())
        self._busy = False

    def _mode_changed(self) -> None:
        self._busy = True
        if self.mode.currentData() == "pct":
            self.w.setValue(100)
            self.h.setValue(100)
        else:
            self.w.setValue(self.orig.width())
            self.h.setValue(self.orig.height())
        self._busy = False

    def _linked(self, from_width: bool) -> None:
        if self._busy or not self.keep.isChecked():
            return
        self._busy = True
        if self.mode.currentData() == "pct":
            (self.h if from_width else self.w).setValue((self.w if from_width else self.h).value())
        elif from_width:
            self.h.setValue(max(1, round(self.w.value() * self.orig.height() / max(1, self.orig.width()))))
        else:
            self.w.setValue(max(1, round(self.h.value() * self.orig.width() / max(1, self.orig.height()))))
        self._busy = False

    def result_size(self) -> QSize:
        if self.mode.currentData() == "pct":
            return QSize(
                max(1, round(self.orig.width() * self.w.value() / 100)), max(1, round(self.orig.height() * self.h.value() / 100))
            )
        return QSize(self.w.value(), self.h.value())
