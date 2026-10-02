"""옵션 대화상자 (일반 / 에디터 / 캡처 / 파일 이름 / 자동 저장 / 이미지 / FTP 설정 / 단축키)."""

from __future__ import annotations

import ftplib
import shutil

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QColor, QPainter, QPaintEvent
from PySide6.QtWidgets import (
    QButtonGroup,
    QCheckBox,
    QColorDialog,
    QComboBox,
    QDialog,
    QFileDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QMessageBox,
    QPushButton,
    QRadioButton,
    QSlider,
    QSpinBox,
    QStackedWidget,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from mikmick.config import DEFAULT_HOTKEYS, Config
from mikmick.icons import icon
from mikmick.utils.filename import IMAGE_FORMATS, expand_pattern

OPTIONS_QSS = """
QListWidget#OptNav { background: white; border: 1px solid #c8c8c8; font-size: 13px; outline: 0; }
QListWidget#OptNav::item { padding: 7px; }
QListWidget#OptNav::item:selected { background: #cfcfcf; color: black; border: 1px solid #a8a8a8; }
QFrame#OptBody { background: white; border: 1px solid #c8c8c8; }
QLabel#OptHeader { background: #ebebeb; padding: 5px 8px; font-weight: bold; color: #444; }
"""

RESULT_CHOICES = [
    ("editor", "믹믹 에디터"),
    ("clipboard", "클립보드"),
    ("file", "파일로 저장"),
    ("autosave", "자동 저장"),
    ("ftp", "FTP 전송"),
    ("program", "외부 프로그램 연결"),
]

HOTKEY_ROWS_CAPTURE = [
    ("fullscreen", "전체화면 캡처하기"),
    ("active_window", "활성화된 윈도우 캡처하기"),
    ("window_control", "윈도우 컨트롤 캡처하기"),
    ("scroll", "자동 스크롤 캡처하기"),
    ("region", "영역을 지정하여 캡처하기"),
    ("fixed", "고정된 사각 영역을 캡처하기"),
    ("freehand", "내 마음대로 캡처하기"),
    ("repeat_last", "마지막 캡처 영역 반복"),
]

HOTKEY_ROWS_TOOLS = [
    ("editor", "믹믹 에디터"),
    ("color_picker", "색상 추출 도구"),
    ("palette", "색상 팔레트"),
    ("magnifier", "돋보기"),
    ("ruler", "눈금자"),
    ("protractor", "각도기"),
    ("crosshair", "십자선"),
    ("whiteboard", "프리젠테이션 도구"),
]

KEY_CHOICES = (
    [("", "없음"), ("Print", "PrintScreen")]
    + [(f"F{i}", f"F{i}") for i in range(1, 13)]
    + [(chr(c), chr(c)) for c in range(ord("A"), ord("Z") + 1)]
    + [(str(i), str(i)) for i in range(10)]
)


def header(text: str) -> QLabel:
    lab = QLabel(text)
    lab.setObjectName("OptHeader")
    return lab


def parse_hotkey(seq: str) -> tuple[bool, bool, bool, str]:
    parts = [p for p in seq.split("+") if p]
    key = ""
    mods = set()
    for part in parts:
        if part in ("Shift", "Ctrl", "Alt"):
            mods.add(part)
        else:
            key = part
    return "Shift" in mods, "Ctrl" in mods, "Alt" in mods, key


def build_hotkey(shift: bool, ctrl: bool, alt: bool, key: str) -> str:
    if not key:
        return ""
    parts = [m for m, on in (("Shift", shift), ("Ctrl", ctrl), ("Alt", alt)) if on]
    return "+".join(parts + [key])


class StartPreview(QWidget):
    """시작 모드 미리보기 그림."""

    def __init__(self):
        super().__init__()
        self.mode = "editor"
        self.setFixedSize(230, 120)

    def set_mode(self, mode: str) -> None:
        self.mode = mode
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        p = QPainter(self)
        p.fillRect(self.rect(), QColor("#0f2a4f"))
        if self.mode == "editor":
            win = QRectF(55, 12, 120, 90)
            p.fillRect(win, QColor("#f3f3f3"))
            p.fillRect(QRectF(win.x(), win.y(), 22, win.height()), QColor("#2b579a"))
            p.setPen(QColor("#888"))
            for i in range(6):
                p.drawLine(int(win.x() + 30), int(win.y() + 18 + i * 11), int(win.x() + 70), int(win.y() + 18 + i * 11))
                p.drawLine(int(win.x() + 78), int(win.y() + 18 + i * 11), int(win.x() + 112), int(win.y() + 18 + i * 11))
        else:
            p.fillRect(QRectF(0, 104, 230, 16), QColor("#1e1e1e"))
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor("#ff5f6d"))
            p.drawEllipse(QRectF(200, 106, 12, 12))
        p.end()


class OptionsDialog(QDialog):
    def __init__(self, config: Config, parent: QWidget | None = None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("옵션")
        self.setWindowIcon(icon("options"))
        self.setStyleSheet(OPTIONS_QSS)
        self.resize(640, 594)
        root = QVBoxLayout(self)
        main = QHBoxLayout()
        self.nav = QListWidget()
        self.nav.setObjectName("OptNav")
        self.nav.setFixedWidth(118)
        main.addWidget(self.nav)
        body = QFrame()
        body.setObjectName("OptBody")
        body_layout = QVBoxLayout(body)
        self.pages = QStackedWidget()
        body_layout.addWidget(self.pages)
        main.addWidget(body, 1)
        root.addLayout(main, 1)

        for title, builder in (
            ("일반", self._page_general),
            ("에디터", self._page_editor),
            ("캡처", self._page_capture),
            ("파일 이름", self._page_filename),
            ("자동 저장", self._page_autosave),
            ("이미지", self._page_image),
            ("FTP 설정", self._page_ftp),
            ("단축키", self._page_hotkeys),
        ):
            self.nav.addItem(title)
            item = self.nav.item(self.nav.count() - 1)
            item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            page = QWidget()
            lay = QVBoxLayout(page)
            lay.setContentsMargins(8, 8, 8, 8)
            builder(lay)
            lay.addStretch(1)
            self.pages.addWidget(page)
        self.nav.currentRowChanged.connect(self.pages.setCurrentIndex)
        self.nav.setCurrentRow(0)

        buttons = QHBoxLayout()
        buttons.addStretch(1)
        ok = QPushButton("확인(&O)")
        ok.setFixedWidth(92)
        ok.clicked.connect(self._accept)
        cancel = QPushButton("취소(&C)")
        cancel.setFixedWidth(92)
        cancel.clicked.connect(self.reject)
        buttons.addWidget(ok)
        buttons.addWidget(cancel)
        root.addLayout(buttons)

    # ---------------------------------------------------------------- 페이지
    def _page_general(self, lay: QVBoxLayout) -> None:
        g = self.config.section("general")
        lay.addWidget(header("프로그램 시작 설정"))
        lay.addWidget(QLabel("프로그램 시작 모드를 선택하세요 :"))
        self.start_mode = QComboBox()
        self.start_mode.addItem("믹믹 에디터", "editor")
        self.start_mode.addItem("알림 영역에만 표시", "tray")
        self.start_mode.setCurrentIndex(max(0, self.start_mode.findData(g.get("start_mode", "editor"))))
        self.start_mode.setFixedWidth(240)
        row = QHBoxLayout()
        row.addSpacing(16)
        row.addWidget(self.start_mode)
        row.addStretch(1)
        lay.addLayout(row)
        preview = StartPreview()
        preview.set_mode(self.start_mode.currentData())
        self.start_mode.currentIndexChanged.connect(lambda: preview.set_mode(self.start_mode.currentData()))
        row = QHBoxLayout()
        row.addSpacing(16)
        row.addWidget(preview)
        row.addStretch(1)
        lay.addLayout(row)
        lay.addSpacing(8)
        self.autostart = QCheckBox("로그인 시 자동 실행(&R)")
        self.autostart.setChecked(bool(g.get("autostart", False)))
        lay.addWidget(self.autostart)
        self.autostart_mode = QComboBox()
        self.autostart_mode.addItem("알림 영역에만 표시", "tray")
        self.autostart_mode.addItem("믹믹 에디터", "editor")
        self.autostart_mode.setCurrentIndex(max(0, self.autostart_mode.findData(g.get("autostart_mode", "tray"))))
        self.autostart_mode.setFixedWidth(240)
        self.autostart_mode.setEnabled(self.autostart.isChecked())
        self.autostart.toggled.connect(self.autostart_mode.setEnabled)
        row = QHBoxLayout()
        row.addSpacing(16)
        row.addWidget(self.autostart_mode)
        row.addStretch(1)
        lay.addLayout(row)
        self.check_updates = QCheckBox("주기적으로 프로그램 업데이트를 확인합니다.")
        self.check_updates.setChecked(bool(g.get("check_updates", True)))
        lay.addWidget(self.check_updates)

    def _page_editor(self, lay: QVBoxLayout) -> None:
        e = self.config.section("editor")
        lay.addWidget(header("믹믹 에디터 설정"))
        self.ed_checks = {}
        for key, label in (
            ("hide_on_capture", "캡처시 믹믹 에디터를 숨기기"),
            ("center_image", "이미지를 화면 가운데에 표시"),
            ("auto_shape_selection", "영역 선택시 자동으로 도형을 배경화하기"),
            ("no_save_prompt", "이미지를 닫을 때 저장 여부를 확인하지 않기"),
            ("quit_on_close", "믹믹 에디터를 닫을 때 프로그램을 종료하기"),
        ):
            chk = QCheckBox(label)
            chk.setChecked(bool(e.get(key, False)))
            lay.addWidget(chk)
            self.ed_checks[key] = chk
        lay.addSpacing(10)
        lay.addWidget(header("이미지 배경색"))
        grid = QGridLayout()
        self.bg_solid = QRadioButton("단색")
        self.bg_grid = QRadioButton("그리드")
        grp = QButtonGroup(self)
        grp.addButton(self.bg_solid)
        grp.addButton(self.bg_grid)
        (self.bg_grid if e.get("bg_mode") == "grid" else self.bg_solid).setChecked(True)
        self.bg_color = QColor(e.get("bg_color", "#dcdcdc"))
        self.bg_btn = QToolButton()
        self.bg_btn.setFixedSize(58, 24)
        self.bg_btn.clicked.connect(self._pick_bg)
        self._refresh_bg()
        grid_sample = QLabel()
        grid_sample.setFixedSize(58, 24)
        grid_sample.setStyleSheet(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #ddd, stop:0.49 #ddd, stop:0.5 #fff, stop:1 #fff);"
            "border: 1px solid #aaa;"
        )
        grid.addWidget(self.bg_solid, 0, 0)
        grid.addWidget(self.bg_btn, 0, 1)
        grid.addWidget(self.bg_grid, 1, 0)
        grid.addWidget(grid_sample, 1, 1)
        grid.setColumnStretch(2, 1)
        grid.setColumnMinimumWidth(0, 140)
        lay.addLayout(grid)

    def _pick_bg(self) -> None:
        c = QColorDialog.getColor(self.bg_color, self, "배경색")
        if c.isValid():
            self.bg_color = c
            self._refresh_bg()

    def _refresh_bg(self) -> None:
        self.bg_btn.setStyleSheet(f"background: {self.bg_color.name()}; border: 1px solid #aaa;")

    def _page_capture(self, lay: QVBoxLayout) -> None:
        c = self.config.section("capture")
        lay.addWidget(header("캡처 결과"))
        lab = QLabel("알림 영역의 아이콘 메뉴를 선택하거나 단축키를 사용할 경우 캡처 결과의 방식을 변경할 수 있습니다.")
        lab.setWordWrap(True)
        lay.addWidget(lab)
        self.result = QComboBox()
        for key, label in RESULT_CHOICES:
            self.result.addItem(label, key)
        self.result.setCurrentIndex(max(0, self.result.findData(c.get("result", "editor"))))
        self.result.setFixedWidth(250)
        lay.addWidget(self.result)
        lay.addSpacing(6)
        lay.addWidget(header("캡처 옵션"))
        self.cap_checks = {}
        for key, label in (
            ("multi_monitor", "다중 모니터 기능 지원"),
            ("sound", "캡처 완료시 효과음을 출력"),
            ("include_cursor", "마우스 커서를 캡처된 이미지에 포함"),
            ("always_clipboard", "캡처된 이미지를 항상 클립보드에 저장"),
            ("show_toolbar", "캡처시 캡처도구모음(안내) 표시"),
        ):
            chk = QCheckBox(label)
            chk.setChecked(bool(c.get(key, False)))
            lay.addWidget(chk)
            self.cap_checks[key] = chk
        row = QHBoxLayout()
        row.addWidget(QLabel("캡처 지연 시간"))
        self.delay = QSpinBox()
        self.delay.setRange(0, 60000)
        self.delay.setSingleStep(500)
        self.delay.setSuffix(" ms")
        self.delay.setValue(int(c.get("delay_ms", 0)))
        row.addWidget(self.delay)
        row.addStretch(1)
        lay.addLayout(row)
        lay.addSpacing(6)
        lay.addWidget(header("스크롤 캡처"))
        row = QHBoxLayout()
        row.addWidget(QLabel("스크롤 지연시간 :"))
        self.scroll_delay = QSpinBox()
        self.scroll_delay.setRange(30, 5000)
        self.scroll_delay.setSingleStep(50)
        self.scroll_delay.setSuffix(" ms")
        self.scroll_delay.setValue(int(c.get("scroll_delay_ms", 100)))
        row.addWidget(self.scroll_delay)
        row.addStretch(1)
        lay.addLayout(row)
        lay.addSpacing(6)
        lay.addWidget(header("화면 확대창"))
        self.magnifier = QCheckBox("캡처시 화면 확대창 표시")
        self.magnifier.setChecked(bool(c.get("magnifier", True)))
        lay.addWidget(self.magnifier)
        row = QHBoxLayout()
        minus = QToolButton()
        minus.setText("−")
        plus = QToolButton()
        plus.setText("+")
        self.zoom = QSlider(Qt.Orientation.Horizontal)
        self.zoom.setRange(2, 12)
        self.zoom.setValue(int(c.get("magnifier_zoom", 6)))
        self.zoom.setFixedWidth(130)
        zoom_label = QLabel()
        self.zoom.valueChanged.connect(lambda v: zoom_label.setText(f"화면 확대 배율 : {v}X"))
        zoom_label.setText(f"화면 확대 배율 : {self.zoom.value()}X")
        minus.clicked.connect(lambda: self.zoom.setValue(self.zoom.value() - 1))
        plus.clicked.connect(lambda: self.zoom.setValue(self.zoom.value() + 1))
        row.addSpacing(16)
        row.addWidget(minus)
        row.addWidget(self.zoom)
        row.addWidget(plus)
        row.addSpacing(10)
        row.addWidget(zoom_label)
        row.addStretch(1)
        lay.addLayout(row)

    def _page_filename(self, lay: QVBoxLayout) -> None:
        f = self.config.section("filename")
        lay.addWidget(header("파일 이름"))
        lay.addWidget(QLabel("자동 저장 기능이나 FTP 전송시 생성할 파일명을 입력하여 주십시오."))
        self.pattern = QComboBox()
        self.pattern.setEditable(True)
        for pat in ("%c", "%y-%m-%d_%h%n%s", "screenshot_%y%m%d_%c", "%u_%t"):
            self.pattern.addItem(pat)
        self.pattern.setEditText(f.get("pattern", "%c"))
        lay.addWidget(self.pattern)
        lay.addSpacing(6)
        ex = QLabel(
            "예시)    %y : 년, %m : 월, %d : 일\n"
            "            %h : 시, %n : 분, %s : 초\n"
            "            %c : 번호\n"
            "            %u : 사용자 이름, %w : 컴퓨터 이름\n"
            "            %t : 유닉스 타임스탬프"
        )
        ex.setStyleSheet("color: #333; margin-left: 20px;")
        lay.addWidget(ex)
        lay.addSpacing(10)
        lay.addWidget(header("파일 형식"))
        lay.addWidget(QLabel("파일 형식"))
        self.fmt = QComboBox()
        for key, label in IMAGE_FORMATS:
            self.fmt.addItem(label, key)
        self.fmt.setCurrentIndex(max(0, self.fmt.findData(f.get("format", "png"))))
        self.fmt.setFixedWidth(300)
        lay.addWidget(self.fmt)
        self.fn_preview = QLabel()
        self.fn_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.fn_preview.setStyleSheet("border: 1px solid #aaa; padding: 6px; color: #1a5fd0; font-weight: bold;")
        lay.addWidget(self.fn_preview)
        self.pattern.editTextChanged.connect(self._update_fn_preview)
        self.fmt.currentIndexChanged.connect(self._update_fn_preview)
        self._update_fn_preview()

    def _update_fn_preview(self) -> None:
        name = expand_pattern(self.pattern.currentText(), int(self.config.get("filename", "counter", 0)))
        self.fn_preview.setText(f"{name}.{self.fmt.currentData()}")

    def _page_autosave(self, lay: QVBoxLayout) -> None:
        a = self.config.section("autosave")
        lay.addWidget(header("자동 저장"))
        self.autosave = QCheckBox("자동 저장 기능 사용")
        self.autosave.setChecked(bool(a.get("enabled", False)))
        lay.addWidget(self.autosave)
        lay.addWidget(QLabel("이미지를 자동으로 저장할 폴더를 선택하십시오."))
        row = QHBoxLayout()
        self.folder = QLineEdit(a.get("folder", ""))
        self.folder.setPlaceholderText("~/Pictures/MikMick")
        browse = QToolButton()
        browse.setText("···")
        browse.clicked.connect(self._browse_folder)
        row.addWidget(self.folder)
        row.addWidget(browse)
        lay.addLayout(row)
        lay.addSpacing(14)
        lay.addWidget(header("외부 프로그램 연결"))
        lay.addWidget(QLabel("캡처 후 연결할 외부 프로그램을 선택하여 주십시오."))
        grid = QGridLayout()
        grid.addWidget(QLabel("프로그램 경로 :"), 0, 0, Qt.AlignmentFlag.AlignRight)
        self.program = QComboBox()
        self.program.setEditable(True)
        self.program.addItem("")
        for prog in ("gimp", "krita", "pinta", "inkscape", "eog", "gwenview", "xdg-open"):
            path = shutil.which(prog)
            if path:
                self.program.addItem(path)
        self.program.setEditText(a.get("program", ""))
        prow = QHBoxLayout()
        prow.addWidget(self.program, 1)
        pbrowse = QToolButton()
        pbrowse.setText("···")
        pbrowse.clicked.connect(self._browse_program)
        prow.addWidget(pbrowse)
        grid.addLayout(prow, 0, 1)
        grid.addWidget(QLabel("매개변수 :"), 1, 0, Qt.AlignmentFlag.AlignRight)
        self.program_args = QLineEdit(a.get("program_args", ""))
        self.program_args.setPlaceholderText("%f : 파일 경로 (생략 시 마지막 인자로 전달)")
        grid.addWidget(self.program_args, 1, 1)
        lay.addLayout(grid)

    def _browse_folder(self) -> None:
        d = QFileDialog.getExistingDirectory(self, "폴더 선택", self.folder.text())
        if d:
            self.folder.setText(d)

    def _browse_program(self) -> None:
        p, _ = QFileDialog.getOpenFileName(self, "프로그램 선택", "/usr/bin")
        if p:
            self.program.setEditText(p)

    def _page_image(self, lay: QVBoxLayout) -> None:
        lay.addWidget(header("JPEG 옵션"))
        row = QHBoxLayout()
        row.addWidget(QLabel("화질"))
        row.addSpacing(30)
        self.quality_preset = QComboBox()
        for label, val in (("최대", 100), ("높음", 90), ("보통", 75), ("낮음", 50), ("사용자 정의", -1)):
            self.quality_preset.addItem(label, val)
        self.quality_preset.setFixedWidth(110)
        self.quality = QSpinBox()
        self.quality.setRange(1, 100)
        self.quality.setValue(int(self.config.get("image", "jpeg_quality", 100)))
        row.addWidget(self.quality_preset)
        row.addWidget(self.quality)
        row.addStretch(1)
        lay.addLayout(row)
        row = QHBoxLayout()
        row.addSpacing(70)
        minus = QToolButton()
        minus.setText("−")
        plus = QToolButton()
        plus.setText("+")
        slider = QSlider(Qt.Orientation.Horizontal)
        slider.setRange(1, 100)
        slider.setValue(self.quality.value())
        slider.setFixedWidth(180)
        slider.valueChanged.connect(self.quality.setValue)
        self.quality.valueChanged.connect(slider.setValue)
        minus.clicked.connect(lambda: self.quality.setValue(self.quality.value() - 1))
        plus.clicked.connect(lambda: self.quality.setValue(self.quality.value() + 1))
        row.addWidget(minus)
        row.addWidget(slider)
        row.addWidget(plus)
        row.addStretch(1)
        lay.addLayout(row)

        def sync_preset(v: int) -> None:
            idx = self.quality_preset.findData(v)
            self.quality_preset.blockSignals(True)
            self.quality_preset.setCurrentIndex(idx if idx >= 0 else self.quality_preset.count() - 1)
            self.quality_preset.blockSignals(False)

        def preset_changed() -> None:
            v = self.quality_preset.currentData()
            if v and v > 0:
                self.quality.setValue(v)

        self.quality.valueChanged.connect(sync_preset)
        self.quality_preset.currentIndexChanged.connect(preset_changed)
        sync_preset(self.quality.value())

    def _page_ftp(self, lay: QVBoxLayout) -> None:
        f = self.config.section("ftp")
        lay.addWidget(header("FTP 서버 설정"))
        grid = QGridLayout()
        self.ftp_server = QLineEdit(f.get("server", ""))
        self.ftp_port = QSpinBox()
        self.ftp_port.setRange(1, 65535)
        self.ftp_port.setValue(int(f.get("port", 21)))
        self.ftp_path = QLineEdit(f.get("path", ""))
        self.ftp_passive = QCheckBox("수동 모드 사용")
        self.ftp_passive.setChecked(bool(f.get("passive", False)))
        self.ftp_user = QLineEdit(f.get("user", "anonymous"))
        self.ftp_pass = QLineEdit(f.get("password", ""))
        self.ftp_pass.setEchoMode(QLineEdit.EchoMode.Password)
        grid.addWidget(QLabel("FTP 서버 :"), 0, 0, Qt.AlignmentFlag.AlignRight)
        grid.addWidget(self.ftp_server, 0, 1)
        grid.addWidget(QLabel("포트 :"), 0, 2, Qt.AlignmentFlag.AlignRight)
        grid.addWidget(self.ftp_port, 0, 3)
        grid.addWidget(QLabel("경로 :"), 1, 0, Qt.AlignmentFlag.AlignRight)
        grid.addWidget(self.ftp_path, 1, 1)
        grid.addWidget(self.ftp_passive, 2, 1)
        grid.setRowMinimumHeight(3, 12)
        grid.addWidget(QLabel("사용자 이름 :"), 4, 0, Qt.AlignmentFlag.AlignRight)
        grid.addWidget(self.ftp_user, 4, 1)
        grid.addWidget(QLabel("비밀번호 :"), 5, 0, Qt.AlignmentFlag.AlignRight)
        grid.addWidget(self.ftp_pass, 5, 1)
        test = QPushButton("접속 테스트")
        test.clicked.connect(self._ftp_test)
        grid.addWidget(test, 5, 2, 1, 2)
        lay.addLayout(grid)
        lay.addSpacing(10)
        box = QFrame()
        box.setFrameShape(QFrame.Shape.StyledPanel)
        bl = QVBoxLayout(box)
        self.ftp_open = QCheckBox("전송 후 웹브라우저로 URL 열기")
        self.ftp_open.setChecked(bool(f.get("open_url", False)))
        self.ftp_copy = QCheckBox("전송 후 클립보드에 URL 복사")
        self.ftp_copy.setChecked(bool(f.get("copy_url", False)))
        bl.addWidget(self.ftp_open)
        bl.addWidget(self.ftp_copy)
        row = QHBoxLayout()
        row.addSpacing(40)
        row.addWidget(QLabel("URL"))
        self.ftp_url = QLineEdit(f.get("url", ""))
        self.ftp_url.setPlaceholderText("https://example.com/screenshots")
        row.addWidget(self.ftp_url)
        bl.addLayout(row)
        lay.addWidget(box)
        lay.addSpacing(10)
        self.ftp_status = QLineEdit()
        self.ftp_status.setReadOnly(True)
        lay.addWidget(self.ftp_status)

    def _ftp_test(self) -> None:
        from mikmick.outputs import ftp_test

        snapshot = self.config.snapshot()
        self._store_ftp()
        try:
            msg = ftp_test(self.config)
            self.ftp_status.setText("접속 성공: " + msg.splitlines()[0])
        except (OSError, ftplib.all_errors) as exc:
            self.ftp_status.setText(f"접속 실패: {exc}")
        finally:
            self.config.restore(snapshot)

    def _page_hotkeys(self, lay: QVBoxLayout) -> None:
        hk = self.config.section("hotkeys")
        self.hotkey_rows: dict[str, tuple[QCheckBox, QCheckBox, QCheckBox, QComboBox]] = {}
        for title, rows in (("화면 캡처 도구", HOTKEY_ROWS_CAPTURE), ("기타 도구", HOTKEY_ROWS_TOOLS)):
            lay.addWidget(header(title))
            grid = QGridLayout()
            grid.setVerticalSpacing(2)
            for r, (key, label) in enumerate(rows):
                shift, ctrl, alt, k = parse_hotkey(hk.get(key, ""))
                grid.addWidget(QLabel(label), r, 0)
                cs = QCheckBox("Shift")
                cc = QCheckBox("Ctrl")
                ca = QCheckBox("Alt")
                cs.setChecked(shift)
                cc.setChecked(ctrl)
                ca.setChecked(alt)
                combo = QComboBox()
                for value, text in KEY_CHOICES:
                    combo.addItem(text, value)
                combo.setCurrentIndex(max(0, combo.findData(k)))
                combo.setFixedWidth(88)

                def toggle(_i=0, boxes=(cs, cc, ca), c=combo):
                    for b in boxes:
                        b.setEnabled(bool(c.currentData()))

                combo.currentIndexChanged.connect(toggle)
                toggle()
                grid.addWidget(cs, r, 1)
                grid.addWidget(cc, r, 2)
                grid.addWidget(ca, r, 3)
                grid.addWidget(combo, r, 4)
                self.hotkey_rows[key] = (cs, cc, ca, combo)
            grid.setColumnStretch(0, 1)
            lay.addLayout(grid)
        row = QHBoxLayout()
        preset = QComboBox()
        preset.addItem("사용자 정의", "custom")
        preset.addItem("기본값", "default")
        preset.addItem("모두 해제", "none")
        preset.setFixedWidth(140)
        preset.activated.connect(lambda _i: self._hotkey_preset(preset.currentData()))
        row.addWidget(preset)
        row.addStretch(1)
        lay.addLayout(row)
        note = QLabel("※ Wayland 세션에서는 전역 단축키를 데스크톱 설정에서 'mikmick --capture region' 등으로 등록하세요.")
        note.setWordWrap(True)
        note.setStyleSheet("color: #777; font-size: 11px;")
        lay.addWidget(note)

    def _hotkey_preset(self, kind: str) -> None:
        if kind == "custom":
            return
        for key, (cs, cc, ca, combo) in self.hotkey_rows.items():
            seq = DEFAULT_HOTKEYS.get(key, "") if kind == "default" else ""
            shift, ctrl, alt, k = parse_hotkey(seq)
            cs.setChecked(shift)
            cc.setChecked(ctrl)
            ca.setChecked(alt)
            combo.setCurrentIndex(max(0, combo.findData(k)))

    # ---------------------------------------------------------------- 저장
    def _store_ftp(self) -> None:
        f = self.config.section("ftp")
        f.update(
            {
                "server": self.ftp_server.text().strip(),
                "port": self.ftp_port.value(),
                "path": self.ftp_path.text().strip(),
                "passive": self.ftp_passive.isChecked(),
                "user": self.ftp_user.text(),
                "password": self.ftp_pass.text(),
                "open_url": self.ftp_open.isChecked(),
                "copy_url": self.ftp_copy.isChecked(),
                "url": self.ftp_url.text().strip(),
            }
        )

    def _accept(self) -> None:
        seen: dict[str, str] = {}
        hotkeys: dict[str, str] = {}
        for key, (cs, cc, ca, combo) in self.hotkey_rows.items():
            seq = build_hotkey(cs.isChecked(), cc.isChecked(), ca.isChecked(), combo.currentData())
            if seq and seq in seen:
                QMessageBox.warning(self, "옵션", f"단축키 '{seq}' 가 중복되었습니다.")
                self.nav.setCurrentRow(7)
                return
            if seq:
                seen[seq] = key
            hotkeys[key] = seq

        c = self.config
        c.section("general").update(
            {
                "start_mode": self.start_mode.currentData(),
                "autostart": self.autostart.isChecked(),
                "autostart_mode": self.autostart_mode.currentData(),
                "check_updates": self.check_updates.isChecked(),
            }
        )
        e = c.section("editor")
        for key, chk in self.ed_checks.items():
            e[key] = chk.isChecked()
        e["bg_mode"] = "grid" if self.bg_grid.isChecked() else "solid"
        e["bg_color"] = self.bg_color.name()
        cap = c.section("capture")
        cap["result"] = self.result.currentData()
        for key, chk in self.cap_checks.items():
            cap[key] = chk.isChecked()
        cap["delay_ms"] = self.delay.value()
        cap["scroll_delay_ms"] = self.scroll_delay.value()
        cap["magnifier"] = self.magnifier.isChecked()
        cap["magnifier_zoom"] = self.zoom.value()
        c.section("filename").update({"pattern": self.pattern.currentText() or "%c", "format": self.fmt.currentData()})
        c.section("autosave").update(
            {
                "enabled": self.autosave.isChecked(),
                "folder": self.folder.text().strip(),
                "program": self.program.currentText().strip(),
                "program_args": self.program_args.text().strip(),
            }
        )
        c.set("image", "jpeg_quality", self.quality.value())
        self._store_ftp()
        c.section("hotkeys").update(hotkeys)
        c.save()
        self.accept()
