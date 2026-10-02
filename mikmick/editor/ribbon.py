"""오피스 스타일 리본 위젯."""

from __future__ import annotations

from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QAction, QColor, QIcon, QMouseEvent, QPainter, QPaintEvent, QPen
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMenu,
    QSizePolicy,
    QStackedWidget,
    QTabBar,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from mikmick.icons import icon

ACCENT = "#2b579a"

RIBBON_QSS = f"""
#RibbonTop {{ background: {ACCENT}; }}
#RibbonTop QToolButton {{ background: transparent; border: none; padding: 2px; }}
#RibbonTop QToolButton:hover {{ background: rgba(255,255,255,40); }}
QTabBar#RibbonTabs {{ background: transparent; }}
QTabBar#RibbonTabs::tab {{
    color: white; background: transparent; padding: 5px 16px; margin: 0; border: none;
    min-width: 30px;
}}
QTabBar#RibbonTabs::tab:hover {{ background: rgba(255,255,255,40); }}
QTabBar#RibbonTabs::tab:selected {{ background: #f3f3f3; color: {ACCENT}; }}
#RibbonBody {{ background: #f3f3f3; border-bottom: 1px solid #d5d5d5; }}
#RibbonBody QToolButton {{
    border: 1px solid transparent; border-radius: 2px; padding: 2px; background: transparent; color: #333;
}}
#RibbonBody QToolButton:hover {{ background: #dcebfc; border-color: #a9cdf5; }}
#RibbonBody QToolButton:checked {{ background: #c5dcf7; border-color: #88b4e6; }}
#RibbonBody QToolButton:disabled {{ color: #a0a0a0; }}
QLabel#RibbonGroupTitle {{ color: #666; font-size: 11px; }}
"""


class RibbonButton(QToolButton):
    def __init__(self, text: str, icon_name: str | QIcon, large: bool = True, menu: QMenu | None = None, checkable: bool = False):
        super().__init__()
        self.setText(text)
        self.setIcon(icon(icon_name) if isinstance(icon_name, str) else icon_name)
        self.setCheckable(checkable)
        self.setAutoRaise(True)
        if large:
            self.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextUnderIcon)
            self.setIconSize(QSize(30, 30))
            self.setMinimumWidth(60 if menu is not None and not checkable else 44)
            self.setFixedHeight(66)
        else:
            self.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextBesideIcon)
            self.setIconSize(QSize(16, 16))
            self.setFixedHeight(22)
        if menu is not None:
            self.setMenu(menu)
            self.setPopupMode(
                QToolButton.ToolButtonPopupMode.MenuButtonPopup if checkable else QToolButton.ToolButtonPopupMode.InstantPopup
            )


class RibbonGroup(QWidget):
    def __init__(self, title: str):
        super().__init__()
        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        body = QVBoxLayout()
        body.setContentsMargins(4, 2, 4, 2)
        body.setSpacing(0)
        self.content = QHBoxLayout()
        self.content.setSpacing(2)
        body.addLayout(self.content, 1)
        label = QLabel(title)
        label.setObjectName("RibbonGroupTitle")
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(label)
        outer.addLayout(body)
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setStyleSheet("color: #d5d5d5;")
        outer.addWidget(sep)

    def add(self, widget: QWidget) -> QWidget:
        self.content.addWidget(widget)
        return widget

    def add_column(self, *widgets: QWidget) -> None:
        col = QVBoxLayout()
        col.setSpacing(1)
        for w in widgets:
            col.addWidget(w)
        col.addStretch(1)
        self.content.addLayout(col)


class RibbonPage(QWidget):
    def __init__(self):
        super().__init__()
        self.layout_ = QHBoxLayout(self)
        self.layout_.setContentsMargins(4, 2, 4, 0)
        self.layout_.setSpacing(0)
        self._stretch_added = False

    def add_group(self, group: RibbonGroup) -> RibbonGroup:
        self.layout_.addWidget(group)
        return group

    def finish(self) -> None:
        if not self._stretch_added:
            self.layout_.addStretch(1)
            self._stretch_added = True


class ColorSwatch(QToolButton):
    """색1 / 색2 표시 버튼."""

    def __init__(self, label: str, color: QColor):
        super().__init__()
        self.color = QColor(color)
        self.label = label
        self.setFixedSize(44, 66)
        self.setCheckable(True)

    def set_color(self, color: QColor) -> None:
        self.color = QColor(color)
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        super().paintEvent(event)
        p = QPainter(self)
        r = self.rect().adjusted(7, 6, -7, -26)
        p.setPen(QPen(QColor("#8a8a8a")))
        p.setBrush(self.color)
        p.drawRect(r)
        p.setPen(QColor("#333") if self.isEnabled() else QColor("#a0a0a0"))
        p.drawText(self.rect().adjusted(0, 0, 0, -6), Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignHCenter, self.label)
        p.end()


class PaletteCell(QToolButton):
    picked = Signal(QColor, bool)

    def __init__(self, color: QColor | None):
        super().__init__()
        self.color = color
        self.setFixedSize(18, 18)
        self.setAutoRaise(False)
        self.setStyleSheet("border: none;")

    def set_color(self, color: QColor | None) -> None:
        self.color = color
        self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if self.color is not None and self.isEnabled():
            self.picked.emit(self.color, event.button() != Qt.MouseButton.RightButton)

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        p.setPen(QColor("#b0b0b0"))
        p.setBrush(self.color if self.color is not None else QColor("#f3f3f3"))
        p.drawRect(self.rect().adjusted(0, 0, -1, -1))
        if self.color is not None:
            p.setPen(QColor("white"))
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawRect(self.rect().adjusted(1, 1, -2, -2))
        p.end()


PALETTE_COLORS = [
    "#000000", "#7f7f7f", "#880015", "#ed1c24", "#ff7f27", "#fff200", "#22b14c", "#00a2e8", "#3f48cc", "#a349a4",
    "#ffffff", "#c3c3c3", "#b97a57", "#ffaec9", "#ffc90e", "#efe4b0", "#b5e61d", "#99d9ea", "#7092be", "#c8bfe7",
]


class PaletteGrid(QWidget):
    picked = Signal(QColor, bool)

    def __init__(self, custom: list[str]):
        super().__init__()
        grid = QGridLayout(self)
        grid.setContentsMargins(2, 4, 2, 2)
        grid.setSpacing(3)
        for i, c in enumerate(PALETTE_COLORS):
            cell = PaletteCell(QColor(c))
            cell.picked.connect(self.picked)
            grid.addWidget(cell, i // 10, i % 10)
        self.custom_cells: list[PaletteCell] = []
        for i in range(10):
            cell = PaletteCell(None)
            cell.picked.connect(self.picked)
            grid.addWidget(cell, 2, i)
            self.custom_cells.append(cell)
        self.set_custom(custom)

    def set_custom(self, colors: list[str]) -> None:
        for i, cell in enumerate(self.custom_cells):
            cell.set_color(QColor(colors[i]) if i < len(colors) else None)


class Ribbon(QWidget):
    file_clicked = Signal()

    def __init__(self):
        super().__init__()
        self.setStyleSheet(RIBBON_QSS)
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        top = QWidget()
        top.setObjectName("RibbonTop")
        top.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.top_layout = QHBoxLayout(top)
        self.top_layout.setContentsMargins(4, 2, 4, 0)
        self.top_layout.setSpacing(2)
        self.quick = QHBoxLayout()
        self.quick.setSpacing(0)
        self.top_layout.addLayout(self.quick)
        self.top_layout.addSpacing(10)
        self.tabs = QTabBar()
        self.tabs.setObjectName("RibbonTabs")
        self.tabs.setDrawBase(False)
        self.tabs.setExpanding(False)
        self.top_layout.addWidget(self.tabs)
        self.top_layout.addStretch(1)
        self.extra = QHBoxLayout()
        self.top_layout.addLayout(self.extra)
        root.addWidget(top)

        self.body = QWidget()
        self.body.setObjectName("RibbonBody")
        self.body.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        body_layout = QHBoxLayout(self.body)
        body_layout.setContentsMargins(0, 0, 0, 0)
        self.stack = QStackedWidget()
        self.stack.setFixedHeight(92)
        body_layout.addWidget(self.stack, 1)
        self.collapse_btn = QToolButton()
        self.collapse_btn.setText("︿")
        self.collapse_btn.setToolTip("리본 축소/확장")
        self.collapse_btn.clicked.connect(self.toggle_collapsed)
        body_layout.addWidget(self.collapse_btn, 0, Qt.AlignmentFlag.AlignBottom)
        root.addWidget(self.body)

        self.tabs.addTab("파일")
        self._last_index = 1
        self.tabs.currentChanged.connect(self._on_tab)
        self.collapsed = False

    def add_quick(self, action: QAction) -> QToolButton:
        btn = QToolButton()
        btn.setDefaultAction(action)
        btn.setIconSize(QSize(16, 16))
        btn.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonIconOnly)
        self.quick.addWidget(btn)
        return btn

    def add_page(self, title: str, page: RibbonPage) -> None:
        page.finish()
        self.tabs.addTab(title)
        self.stack.addWidget(page)
        if self.tabs.count() == 2:
            self.tabs.setCurrentIndex(1)

    def _on_tab(self, index: int) -> None:
        if index == 0:
            self.tabs.blockSignals(True)
            self.tabs.setCurrentIndex(self._last_index)
            self.tabs.blockSignals(False)
            self.file_clicked.emit()
            return
        self._last_index = index
        self.stack.setCurrentIndex(index - 1)
        if self.collapsed:
            self.toggle_collapsed()

    def toggle_collapsed(self) -> None:
        self.collapsed = not self.collapsed
        self.stack.setVisible(not self.collapsed)
        self.collapse_btn.setText("﹀" if self.collapsed else "︿")

    def set_body_enabled(self, enabled: bool, keep: list[QWidget] | None = None) -> None:
        for i in range(self.stack.count()):
            page = self.stack.widget(i)
            for btn in page.findChildren(QToolButton):
                btn.setEnabled(enabled or (keep is not None and btn in keep))
            for w in page.findChildren(PaletteCell):
                w.setEnabled(enabled)


def spacer() -> QWidget:
    w = QWidget()
    w.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
    return w
