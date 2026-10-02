"""'파일' 탭 백스테이지 화면 (시작 / 새로 만들기 / 공유 / 썸네일 / 정보)."""

from __future__ import annotations

from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QColor, QFont, QGuiApplication, QPixmap
from PySide6.QtWidgets import (
    QCheckBox,
    QColorDialog,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QSpinBox,
    QStackedWidget,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from mikmick import APP_NAME_KO, __version__
from mikmick.capture.manager import MODE_LABELS
from mikmick.icons import icon, render

ACCENT = "#2b579a"

BACKSTAGE_QSS = f"""
#Backstage {{ background: #f3f3f3; }}
#BackstageSide {{ background: {ACCENT}; }}
#BackstageSide QPushButton {{
    color: white; background: transparent; border: none; text-align: left; padding: 9px 18px; font-size: 12px;
}}
#BackstageSide QPushButton:hover {{ background: rgba(255,255,255,45); }}
#BackstageSide QPushButton:checked {{ background: rgba(255,255,255,70); }}
#BackstageSide QPushButton:disabled {{ color: rgba(255,255,255,90); }}
#BackstageSide QToolButton {{ background: transparent; border: none; }}
QLabel#PageTitle {{ font-size: 27px; color: #444; font-weight: 300; }}
QLabel#SectionTitle {{ font-size: 15px; color: {ACCENT}; }}
QLabel#ItemDesc {{ color: #555; font-size: 11px; }}
QToolButton#TaskItem {{ border: 1px solid transparent; text-align: left; padding: 4px; background: transparent; }}
QToolButton#TaskItem:hover {{ background: #dcebfc; border-color: #a9cdf5; }}
QToolButton#BigButton {{ border: 1px solid #c8c8c8; background: #f8f8f8; padding: 6px; }}
QToolButton#BigButton:hover {{ background: #dcebfc; border-color: #a9cdf5; }}
"""


def _title(text: str) -> QLabel:
    lab = QLabel(text)
    lab.setObjectName("PageTitle")
    return lab


def _section(text: str) -> QLabel:
    lab = QLabel(text)
    lab.setObjectName("SectionTitle")
    return lab


def _hline() -> QFrame:
    line = QFrame()
    line.setFrameShape(QFrame.Shape.HLine)
    line.setStyleSheet("color: #d0d0d0;")
    return line


class TaskItem(QToolButton):
    """아이콘 + 제목 + 설명으로 구성된 시작 화면 항목."""

    def __init__(self, icon_name: str, title: str, desc: str):
        super().__init__()
        self.setObjectName("TaskItem")
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setFixedHeight(46 if desc else 40)
        lay = QHBoxLayout(self)
        lay.setContentsMargins(6, 2, 6, 2)
        lay.setSpacing(12)
        ic = QLabel()
        ic.setPixmap(render(icon_name, 30))
        ic.setFixedSize(32, 32)
        lay.addWidget(ic)
        texts = QVBoxLayout()
        texts.setSpacing(0)
        t = QLabel(title)
        t.setStyleSheet("font-size: 12px; color: #222;")
        texts.addWidget(t)
        if desc:
            d = QLabel(desc)
            d.setObjectName("ItemDesc")
            texts.addWidget(d)
        lay.addLayout(texts, 1)
        for child in (ic, t):
            child.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents)


class BigButton(QToolButton):
    def __init__(self, icon_name: str, text: str):
        super().__init__()
        self.setObjectName("BigButton")
        self.setIcon(icon(icon_name))
        self.setIconSize(QSize(32, 32))
        self.setText(text)
        self.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextUnderIcon)
        self.setFixedSize(96, 76)


CAPTURE_ITEMS = [
    ("fullscreen", "cap_fullscreen"),
    ("active_window", "cap_window"),
    ("window_control", "cap_control"),
    ("scroll", "cap_scroll"),
    ("region", "cap_region"),
    ("fixed", "cap_fixed"),
    ("freehand", "cap_freehand"),
    ("repeat_last", "cap_repeat"),
]

TOOL_ITEMS = [
    ("color_picker", "tool_color_picker", "색상 추출 도구", "화면으로부터 색상코드를 추출합니다."),
    ("palette", "tool_palette", "색상 팔레트", "색상코드를 편집하거나 형식을 변경합니다."),
    ("magnifier", "tool_magnifier", "돋보기", "화면을 확대하는 돋보기를 표시합니다."),
    ("ruler", "tool_ruler", "눈금자", "화면의 객체를 픽셀이나 인치로 표시합니다."),
    ("crosshair", "tool_crosshair", "십자선", "화면의 좌표 및 상대좌표를 계산합니다."),
    ("protractor", "tool_protractor", "각도기", "화면의 기울기나 각도를 계산합니다."),
    ("whiteboard", "tool_whiteboard", "프리젠테이션 도구", "화면에 선이나 도형을 직접 그릴 수 있습니다."),
]

PRESETS = [
    ("clipboard", "클립보드", None),
    ("screen", "화면 크기", None),
    ("640x480", "640 x 480", QSize(640, 480)),
    ("800x600", "800 x 600", QSize(800, 600)),
    ("1024x768", "1024 x 768", QSize(1024, 768)),
    ("1280x720", "1280 x 720 (HD)", QSize(1280, 720)),
    ("1920x1080", "1920 x 1080 (Full HD)", QSize(1920, 1080)),
    ("a4", "A4 (96 DPI)", QSize(794, 1123)),
    ("custom", "사용자 정의", None),
]


class StartPage(QWidget):
    action = Signal(str, str)  # kind(new|open|capture|tool), name
    hide_changed = Signal(bool)

    def __init__(self, hide_start: bool):
        super().__init__()
        root = QVBoxLayout(self)
        root.setContentsMargins(40, 30, 30, 14)
        root.addWidget(_title("작업을 선택하십시오."))
        root.addSpacing(14)
        cols = QHBoxLayout()
        cols.setSpacing(20)

        left = QVBoxLayout()
        left.setSpacing(2)
        left.addWidget(_section("새 작업"))
        for kind, ic, title, desc in (
            ("new", "new", "새로 만들기", "새 이미지를 만듭니다."),
            ("open", "open", "열기", "기존 이미지 파일을 선택하여 불러옵니다."),
        ):
            item = TaskItem(ic, title, desc)
            item.clicked.connect(lambda _=False, k=kind: self.action.emit(k, ""))
            left.addWidget(item)
        left.addSpacing(4)
        left.addWidget(_hline())
        left.addSpacing(4)
        left.addWidget(_section("화면 캡처 도구"))
        for mode, ic in CAPTURE_ITEMS:
            item = TaskItem(ic, MODE_LABELS[mode], "")
            item.setFixedHeight(38)
            item.clicked.connect(lambda _=False, m=mode: self.action.emit("capture", m))
            left.addWidget(item)
        left.addStretch(1)
        cols.addLayout(left, 1)

        vline = QFrame()
        vline.setFrameShape(QFrame.Shape.VLine)
        vline.setStyleSheet("color: #d0d0d0;")
        cols.addWidget(vline)

        right = QVBoxLayout()
        right.setSpacing(2)
        right.addWidget(_section("그래픽 도구"))
        for name, ic, title, desc in TOOL_ITEMS:
            item = TaskItem(ic, title, desc)
            item.clicked.connect(lambda _=False, n=name: self.action.emit("tool", n))
            right.addWidget(item)
        right.addStretch(1)
        cols.addLayout(right, 1)
        root.addLayout(cols, 1)

        bottom = QHBoxLayout()
        bottom.addStretch(1)
        self.hide_check = QCheckBox("시작할 때 이 창을 표시 안함")
        self.hide_check.setChecked(hide_start)
        self.hide_check.toggled.connect(self.hide_changed)
        bottom.addWidget(self.hide_check)
        root.addLayout(bottom)


class NewPage(QWidget):
    create = Signal(QSize, QColor)

    def __init__(self, preset: str, size: QSize, bg: QColor):
        super().__init__()
        self.bg = QColor(bg)
        root = QVBoxLayout(self)
        root.setContentsMargins(40, 30, 30, 20)
        root.addWidget(_title("새로 만들기"))
        root.addSpacing(14)

        head = QHBoxLayout()
        btn = BigButton("new", "새로 만들기")
        btn.clicked.connect(self._create)
        head.addWidget(btn)
        info = QVBoxLayout()
        lab = QLabel("새 이미지를 만듭니다.")
        lab.setStyleSheet("font-size: 15px; color: #333;")
        info.addWidget(lab)
        self.size_label = QLabel()
        self.size_label.setObjectName("ItemDesc")
        info.addWidget(self.size_label)
        info.addStretch(1)
        head.addLayout(info)
        head.addStretch(1)
        root.addLayout(head)
        root.addSpacing(6)
        line = _hline()
        line.setFixedWidth(290)
        root.addWidget(line)
        root.addSpacing(6)

        root.addWidget(_section("캔버스 크기"))
        root.addWidget(QLabel("프리셋:"))
        self.preset = QComboBox()
        self.preset.setFixedWidth(290)
        for key, label, _s in PRESETS:
            self.preset.addItem(label, key)
        root.addWidget(self.preset)
        root.addSpacing(8)
        root.addWidget(QLabel("캔버스 크기:"))
        grid = QGridLayout()
        grid.addWidget(QLabel("가로 (Width)"), 0, 0)
        self.w = QSpinBox()
        self.w.setRange(1, 20000)
        self.w.setFixedWidth(75)
        grid.addWidget(self.w, 1, 0)
        swap = QToolButton()
        swap.setIcon(icon("swap"))
        swap.setAutoRaise(True)
        swap.setToolTip("가로/세로 바꾸기")
        swap.clicked.connect(self._swap)
        grid.addWidget(swap, 1, 1, Qt.AlignmentFlag.AlignLeft)
        grid.addWidget(QLabel("세로 (Height)"), 2, 0)
        self.h = QSpinBox()
        self.h.setRange(1, 20000)
        self.h.setFixedWidth(75)
        grid.addWidget(self.h, 3, 0)
        grid.setColumnStretch(2, 1)
        root.addLayout(grid)
        root.addSpacing(8)
        root.addWidget(_section("배경색"))
        self.bg_btn = QToolButton()
        self.bg_btn.setFixedSize(76, 54)
        self.bg_btn.clicked.connect(self._pick_bg)
        root.addWidget(self.bg_btn)
        root.addStretch(1)

        self.w.setValue(size.width())
        self.h.setValue(size.height())
        idx = self.preset.findData(preset)
        self.preset.setCurrentIndex(max(0, idx))
        self.preset.currentIndexChanged.connect(self._apply_preset)
        self.w.valueChanged.connect(self._manual)
        self.h.valueChanged.connect(self._manual)
        self._update_bg()
        self._apply_preset()

    def _apply_preset(self) -> None:
        key = self.preset.currentData()
        size = None
        if key == "clipboard":
            img = QGuiApplication.clipboard().image()
            if not img.isNull():
                size = img.size()
        elif key == "screen":
            screen = QGuiApplication.primaryScreen()
            if screen is not None:
                size = screen.size()
        else:
            for k, _l, s in PRESETS:
                if k == key and s is not None:
                    size = s
        if size is not None:
            for sb, v in ((self.w, size.width()), (self.h, size.height())):
                sb.blockSignals(True)
                sb.setValue(v)
                sb.blockSignals(False)
        self._update_label()

    def _manual(self) -> None:
        self.preset.blockSignals(True)
        self.preset.setCurrentIndex(self.preset.findData("custom"))
        self.preset.blockSignals(False)
        self._update_label()

    def _swap(self) -> None:
        w, h = self.w.value(), self.h.value()
        self.w.setValue(h)
        self.h.setValue(w)

    def _update_label(self) -> None:
        self.size_label.setText(f"캔버스 크기 : {self.w.value()} x {self.h.value()}")

    def _pick_bg(self) -> None:
        c = QColorDialog.getColor(self.bg, self, "배경색", QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if c.isValid():
            self.bg = c
            self._update_bg()

    def _update_bg(self) -> None:
        self.bg_btn.setStyleSheet(f"background: {self.bg.name(QColor.NameFormat.HexArgb)}; border: 1px solid #b0b0b0;")

    def _create(self) -> None:
        self.create.emit(QSize(self.w.value(), self.h.value()), self.bg)

    def preset_key(self) -> str:
        return self.preset.currentData()


class SharePage(QWidget):
    share = Signal(str)

    def __init__(self):
        super().__init__()
        root = QVBoxLayout(self)
        root.setContentsMargins(40, 30, 30, 20)
        root.addWidget(_title("공유"))
        root.addSpacing(14)
        for key, ic, title, desc in (
            ("clipboard", "copy", "클립보드로 복사", "이미지를 클립보드에 복사합니다."),
            ("email", "email", "이메일로 보내기", "기본 메일 프로그램으로 이미지를 첨부하여 보냅니다."),
            ("ftp", "ftp", "FTP 전송", "옵션에 설정된 FTP 서버로 이미지를 전송합니다."),
            ("program", "program", "외부 프로그램 연결", "옵션에 설정된 외부 프로그램으로 이미지를 엽니다."),
            ("default_app", "open", "기본 프로그램으로 열기", "시스템 기본 이미지 프로그램으로 엽니다."),
        ):
            row = QHBoxLayout()
            btn = BigButton(ic, title.split(" ")[0])
            btn.clicked.connect(lambda _=False, k=key: self.share.emit(k))
            row.addWidget(btn)
            texts = QVBoxLayout()
            t = QLabel(title)
            t.setStyleSheet("font-size: 15px; color: #333;")
            texts.addWidget(t)
            d = QLabel(desc)
            d.setObjectName("ItemDesc")
            texts.addWidget(d)
            texts.addStretch(1)
            row.addLayout(texts)
            row.addStretch(1)
            root.addLayout(row)
            root.addSpacing(6)
        root.addStretch(1)


class ThumbnailPage(QWidget):
    activate = Signal(int)

    def __init__(self):
        super().__init__()
        root = QVBoxLayout(self)
        root.setContentsMargins(40, 30, 30, 20)
        root.addWidget(_title("썸네일"))
        root.addSpacing(14)
        self.area = QScrollArea()
        self.area.setWidgetResizable(True)
        self.area.setFrameShape(QFrame.Shape.NoFrame)
        root.addWidget(self.area, 1)

    def set_documents(self, docs: list[tuple[str, QPixmap]]) -> None:
        host = QWidget()
        grid = QGridLayout(host)
        grid.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        for i, (title, pm) in enumerate(docs):
            btn = QToolButton()
            btn.setObjectName("BigButton")
            btn.setIcon(pm.scaled(160, 120, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation))
            btn.setIconSize(QSize(160, 120))
            btn.setText(title)
            btn.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextUnderIcon)
            btn.setFixedSize(180, 160)
            btn.clicked.connect(lambda _=False, idx=i: self.activate.emit(idx))
            grid.addWidget(btn, i // 4, i % 4)
        if not docs:
            grid.addWidget(QLabel("열려 있는 이미지가 없습니다."), 0, 0)
        self.area.setWidget(host)


class InfoPage(QWidget):
    homepage = Signal()
    check_update = Signal()

    def __init__(self):
        super().__init__()
        root = QVBoxLayout(self)
        root.setContentsMargins(40, 30, 30, 20)
        root.addWidget(_title("정보"))
        root.addSpacing(14)
        cols = QHBoxLayout()
        left = QVBoxLayout()
        left.addWidget(_section("프로그램 정보"))
        head = QHBoxLayout()
        logo = QLabel()
        logo.setPixmap(render("logo", 64))
        head.addWidget(logo)
        names = QVBoxLayout()
        n = QLabel(APP_NAME_KO + " (MikMick)")
        f = QFont()
        f.setPixelSize(18)
        n.setFont(f)
        names.addWidget(n)
        names.addWidget(QLabel(f"버전 {__version__}"))
        names.addStretch(1)
        head.addLayout(names)
        head.addStretch(1)
        left.addLayout(head)
        notice = QLabel(
            "믹믹은 리눅스를 위한 무료 오픈소스 화면 캡처 및 이미지 편집 프로그램입니다. "
            "MIT 라이선스에 따라 누구나 자유롭게 사용, 수정, 배포할 수 있습니다. "
            "믹믹은 PicPick(NGWIN)과 제휴 관계가 없는 독립 프로젝트입니다."
        )
        notice.setWordWrap(True)
        notice.setObjectName("ItemDesc")
        notice.setMaximumWidth(290)
        left.addWidget(notice)
        line = _hline()
        line.setFixedWidth(290)
        left.addWidget(line)
        for ic, text, title, desc, sig in (
            ("info", "공식 웹사이트", "공식 웹사이트", "믹믹 GitHub 저장소에 연결합니다.", self.homepage),
            ("update", "업데이트 확인", "업데이트 확인", "최신 버전 업데이트 여부를 확인합니다.", self.check_update),
        ):
            row = QHBoxLayout()
            btn = BigButton(ic, text)
            btn.clicked.connect(sig)
            row.addWidget(btn)
            texts = QVBoxLayout()
            t = QLabel(title)
            t.setStyleSheet("font-size: 15px; color: #333;")
            texts.addWidget(t)
            d = QLabel(desc)
            d.setObjectName("ItemDesc")
            texts.addWidget(d)
            texts.addStretch(1)
            row.addLayout(texts)
            row.addStretch(1)
            left.addLayout(row)
            left.addSpacing(6)
        left.addStretch(1)
        cols.addLayout(left, 1)
        vline = QFrame()
        vline.setFrameShape(QFrame.Shape.VLine)
        vline.setStyleSheet("color: #d0d0d0;")
        cols.addWidget(vline)
        right = QVBoxLayout()
        right.addWidget(_section("라이선스 정보"))
        lic = QLabel("MIT License\n오픈소스 - 등록 불필요")
        lic.setStyleSheet("font-size: 14px; color: #333;")
        right.addWidget(lic)
        right.addStretch(1)
        cols.addLayout(right, 1)
        root.addLayout(cols, 1)


class Backstage(QWidget):
    back = Signal()
    command = Signal(str, str)  # (kind, name)

    SIDE_ITEMS = [
        ("start", "시작"),
        ("new", "새로 만들기"),
        ("open", "열기"),
        ("save", "저장"),
        ("save_as", "다른 이름으로 저장"),
        ("print", "인쇄"),
        ("share", "공유"),
        ("thumbnail", "썸네일"),
        ("close", "닫기"),
        (None, None),
        ("options", "옵션"),
        ("info", "정보"),
    ]
    PAGE_ITEMS = ("start", "new", "share", "thumbnail", "info")
    DOC_ITEMS = ("save", "save_as", "print", "share", "thumbnail", "close")

    def __init__(self, hide_start: bool, preset: str, size: QSize, bg: QColor):
        super().__init__()
        self.setObjectName("Backstage")
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.setStyleSheet(BACKSTAGE_QSS)
        root = QHBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        side = QWidget()
        side.setObjectName("BackstageSide")
        side.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        side.setFixedWidth(150)
        side_layout = QVBoxLayout(side)
        side_layout.setContentsMargins(0, 18, 0, 18)
        side_layout.setSpacing(0)
        back = QToolButton()
        back.setIcon(icon("back"))
        back.setIconSize(QSize(34, 34))
        back.clicked.connect(self.back)
        back.setToolTip("돌아가기")
        side_layout.addWidget(back, 0, Qt.AlignmentFlag.AlignLeft)
        back.setStyleSheet("margin-left: 16px;")
        side_layout.addSpacing(18)
        self.buttons: dict[str, QPushButton] = {}
        for key, label in self.SIDE_ITEMS:
            if key is None:
                side_layout.addSpacing(36)
                continue
            btn = QPushButton(label)
            btn.setCheckable(key in self.PAGE_ITEMS)
            btn.setAutoExclusive(False)
            btn.clicked.connect(lambda _=False, k=key: self._on_side(k))
            side_layout.addWidget(btn)
            self.buttons[key] = btn
        side_layout.addStretch(1)
        root.addWidget(side)

        self.stack = QStackedWidget()
        self.start_page = StartPage(hide_start)
        self.new_page = NewPage(preset, size, bg)
        self.share_page = SharePage()
        self.thumb_page = ThumbnailPage()
        self.info_page = InfoPage()
        self.pages = {
            "start": self.start_page,
            "new": self.new_page,
            "share": self.share_page,
            "thumbnail": self.thumb_page,
            "info": self.info_page,
        }
        for page in self.pages.values():
            self.stack.addWidget(page)
        root.addWidget(self.stack, 1)

        self.start_page.action.connect(self._on_start_action)
        self.new_page.create.connect(lambda size, color: self.command.emit("create", f"{size.width()}x{size.height()}:{color.name(QColor.NameFormat.HexArgb)}"))
        self.share_page.share.connect(lambda k: self.command.emit("share", k))
        self.thumb_page.activate.connect(lambda i: self.command.emit("activate", str(i)))
        self.info_page.homepage.connect(lambda: self.command.emit("homepage", ""))
        self.info_page.check_update.connect(lambda: self.command.emit("check_update", ""))
        self.show_page("start")

    def _on_start_action(self, kind: str, name: str) -> None:
        if kind == "new":
            self.show_page("new")
        else:
            self.command.emit(kind, name)

    def _on_side(self, key: str) -> None:
        if key in self.PAGE_ITEMS:
            self.show_page(key)
            if key == "thumbnail":
                self.command.emit("thumbnails", "")
        else:
            self.buttons[key].setChecked(False)
            self.command.emit(key, "")

    def show_page(self, key: str) -> None:
        for k, btn in self.buttons.items():
            if btn.isCheckable():
                btn.setChecked(k == key)
        self.stack.setCurrentWidget(self.pages[key])

    def set_has_document(self, has_doc: bool) -> None:
        for key in self.DOC_ITEMS:
            self.buttons[key].setEnabled(has_doc)
