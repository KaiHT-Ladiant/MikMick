"""믹믹 에디터 메인 윈도우."""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

from PySide6.QtCore import QPoint, QRect, QSize, Qt, QTimer
from PySide6.QtGui import (
    QAction,
    QActionGroup,
    QBrush,
    QCloseEvent,
    QColor,
    QDragEnterEvent,
    QDropEvent,
    QGuiApplication,
    QImage,
    QImageReader,
    QKeySequence,
    QPainter,
    QPixmap,
)
from PySide6.QtWidgets import (
    QColorDialog,
    QFileDialog,
    QFontDialog,
    QLabel,
    QMainWindow,
    QMenu,
    QMessageBox,
    QSlider,
    QStackedWidget,
    QStatusBar,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from mikmick import APP_NAME_KO
from mikmick.config import pictures_dir
from mikmick.editor import effects
from mikmick.editor.backstage import Backstage
from mikmick.editor.canvas import CanvasView, Document, ToolState
from mikmick.editor.dialogs import ResizeDialog, ask
from mikmick.editor.items import SHAPE_KINDS, SHAPE_LABELS
from mikmick.editor.ribbon import ColorSwatch, PaletteGrid, Ribbon, RibbonButton, RibbonGroup, RibbonPage
from mikmick.icons import icon
from mikmick.utils.filename import IMAGE_FORMATS, ext_of

if TYPE_CHECKING:
    from mikmick.app import Controller


def _grid_brush() -> QBrush:
    pm = QPixmap(16, 16)
    pm.fill(QColor("#ffffff"))
    p = QPainter(pm)
    p.fillRect(0, 0, 8, 8, QColor("#cccccc"))
    p.fillRect(8, 8, 8, 8, QColor("#cccccc"))
    p.end()
    return QBrush(pm)


class EditorWindow(QMainWindow):
    def __init__(self, controller: Controller):
        super().__init__()
        self.ctl = controller
        self.config = controller.config
        self.state = ToolState()
        self._stamp_no = 0
        self.setWindowTitle(APP_NAME_KO)
        self.setWindowIcon(icon("logo"))
        self.resize(1100, 720)
        self.setAcceptDrops(True)

        self._build_actions()
        self.stack = QStackedWidget()
        self.setCentralWidget(self.stack)

        editor = QWidget()
        lay = QVBoxLayout(editor)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)
        self.ribbon = Ribbon()
        self._build_ribbon()
        lay.addWidget(self.ribbon)
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        self.tabs.setTabsClosable(True)
        self.tabs.setMovable(True)
        self.tabs.tabCloseRequested.connect(self.close_document)
        self.tabs.currentChanged.connect(self._on_tab_changed)
        self.empty = QWidget()
        self.empty.setAutoFillBackground(True)
        self.canvas_stack = QStackedWidget()
        self.canvas_stack.addWidget(self.empty)
        self.canvas_stack.addWidget(self.tabs)
        lay.addWidget(self.canvas_stack, 1)
        self.stack.addWidget(editor)

        ni = self.config.section("new_image")
        self.backstage = Backstage(
            bool(self.config.get("general", "hide_start_page", False)),
            ni.get("preset", "clipboard"),
            QSize(int(ni.get("width", 1131)), int(ni.get("height", 720))),
            QColor(ni.get("bg", "#ffffff")),
        )
        self.backstage.back.connect(self.hide_backstage)
        self.backstage.command.connect(self._on_backstage)
        self.backstage.start_page.hide_changed.connect(self._on_hide_start)
        self.stack.addWidget(self.backstage)

        self._build_status()
        self.apply_settings()
        self._update_ui()

    # ------------------------------------------------------------------ 구성
    def _act(self, text: str, icon_name: str | None, slot, shortcut: str | QKeySequence | None = None) -> QAction:
        a = QAction(text, self)
        if icon_name:
            a.setIcon(icon(icon_name))
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
            a.setShortcutContext(Qt.ShortcutContext.WindowShortcut)
        a.triggered.connect(lambda _=False: slot())
        self.addAction(a)
        return a

    def _build_actions(self) -> None:
        self.a_new = self._act("새로 만들기", "new", lambda: self.show_backstage("new"), QKeySequence.StandardKey.New)
        self.a_open = self._act("열기", "open", self.open_files, QKeySequence.StandardKey.Open)
        self.a_save = self._act("저장", "save", self.save, QKeySequence.StandardKey.Save)
        self.a_save_as = self._act("다른 이름으로 저장", "save_as", self.save_as, QKeySequence("Ctrl+Shift+S"))
        self.a_print = self._act("인쇄", "print", self.print_image, QKeySequence.StandardKey.Print)
        self.a_undo = self._act("실행 취소", "undo", self.undo, QKeySequence.StandardKey.Undo)
        self.a_redo = self._act("다시 실행", "redo", self.redo, QKeySequence("Ctrl+Y"))
        self.a_paste = self._act("붙여넣기", "paste", self.paste, QKeySequence.StandardKey.Paste)
        self.a_paste_new = self._act("새 이미지로 붙여넣기", "new", self.paste_as_new, QKeySequence("Ctrl+Shift+V"))
        self.a_cut = self._act("잘라내기", "cut", self.cut, QKeySequence.StandardKey.Cut)
        self.a_copy = self._act("복사", "copy", self.copy, QKeySequence.StandardKey.Copy)
        self.a_delete = self._act("삭제", None, self.delete, QKeySequence.StandardKey.Delete)
        self.a_select_all = self._act("모두 선택", None, self.select_all, QKeySequence.StandardKey.SelectAll)
        self.a_deselect = self._act("선택 해제", None, self.deselect, QKeySequence("Ctrl+D"))
        self.a_crop = self._act("자르기", "crop", self.crop, QKeySequence("Ctrl+Shift+X"))
        self.a_zoom_in = self._act("확대", "zoom_in", lambda: self._zoom_by(1.25), QKeySequence.StandardKey.ZoomIn)
        self.a_zoom_out = self._act("축소", "zoom_out", lambda: self._zoom_by(0.8), QKeySequence.StandardKey.ZoomOut)
        self.a_zoom_100 = self._act("100%", "zoom_100", lambda: self._set_zoom(1.0), QKeySequence("Ctrl+0"))
        self.a_zoom_fit = self._act("화면에 맞추기", "zoom_fit", self._fit, QKeySequence("Ctrl+9"))
        self.a_merge = self._act("개체 병합", "thumbnail", self.merge_objects, QKeySequence("Ctrl+M"))
        self.a_close = self._act("닫기", "close", lambda: self.close_document(self.tabs.currentIndex()), QKeySequence("Ctrl+W"))
        self.doc_actions = [
            self.a_save, self.a_save_as, self.a_print, self.a_cut, self.a_copy, self.a_delete,
            self.a_select_all, self.a_deselect, self.a_crop, self.a_zoom_in, self.a_zoom_out, self.a_zoom_100,
            self.a_zoom_fit, self.a_merge, self.a_close,
        ]

    def _menu(self, items: list[tuple[str, str | None, object] | None]) -> QMenu:
        menu = QMenu(self)
        for item in items:
            if item is None:
                menu.addSeparator()
                continue
            text, ic, slot = item
            a = menu.addAction(icon(ic), text) if ic else menu.addAction(text)
            a.triggered.connect(lambda _=False, s=slot: s())
        return menu

    def _build_ribbon(self) -> None:
        rb = self.ribbon
        for a in (self.a_new, self.a_open, self.a_save, self.a_print, self.a_undo, self.a_redo):
            rb.add_quick(a)
        heart = RibbonButton("", "heart", large=False)
        heart.setToolTip("믹믹 GitHub 저장소")
        heart.clicked.connect(self.ctl.homepage)
        rb.extra.addWidget(heart)

        # 홈 -------------------------------------------------------------
        home = RibbonPage()
        g = home.add_group(RibbonGroup("클립보드"))
        paste = RibbonButton("붙여넣기", "paste", menu=self._menu([
            ("붙여넣기", "paste", self.paste),
            ("새 이미지로 붙여넣기", "new", self.paste_as_new),
        ]))
        g.add(paste)
        self.btn_cut = RibbonButton("잘라내기", "cut", large=False)
        self.btn_cut.clicked.connect(self.cut)
        self.btn_copy = RibbonButton("복사", "copy", large=False)
        self.btn_copy.clicked.connect(self.copy)
        g.add_column(self.btn_cut, self.btn_copy)

        g = home.add_group(RibbonGroup("이미지"))
        effects_menu = self._menu([
            ("흐리게...", "blur", self.fx_blur),
            ("선명하게", None, lambda: self._fx("선명하게", lambda img, r: effects.sharpen(img, 1.0, r))),
            ("모자이크...", "mosaic", self.fx_mosaic),
            None,
            ("무채화", None, lambda: self._fx("무채화", lambda img, r: effects.grayscale(img, r))),
            ("색반전", None, lambda: self._fx("색반전", lambda img, r: effects.invert(img, r))),
            ("밝기/대비...", None, self.fx_brightness),
            ("색조/채도...", None, self.fx_hue),
            None,
            ("테두리...", None, self.fx_border),
            ("그림자", None, self.fx_shadow),
            ("워터마크...", None, self.fx_watermark),
        ])
        b_fx = RibbonButton("효과", "effect", large=False, menu=effects_menu)
        b_resize = RibbonButton("크기 조절", "resize", large=False, menu=self._menu([
            ("이미지 크기 조절...", "resize", self.resize_image),
            ("캔버스 크기 조절...", None, self.resize_canvas),
        ]))
        b_rotate = RibbonButton("회전", "rotate", large=False, menu=self._menu([
            ("오른쪽으로 90° 회전", None, lambda: self._transform("회전", lambda img: effects.rotate(img, 90))),
            ("왼쪽으로 90° 회전", None, lambda: self._transform("회전", lambda img: effects.rotate(img, -90))),
            ("180° 회전", None, lambda: self._transform("회전", lambda img: effects.rotate(img, 180))),
            None,
            ("좌우 대칭", None, lambda: self._transform("좌우 대칭", lambda img: effects.flip(img, True))),
            ("상하 대칭", None, lambda: self._transform("상하 대칭", lambda img: effects.flip(img, False))),
        ]))
        g.add_column(b_fx, b_resize, b_rotate)
        b_crop = RibbonButton("자르기", "crop", large=False)
        b_crop.clicked.connect(self.crop)
        g.add_column(b_crop)

        g = home.add_group(RibbonGroup("도구"))
        self.tool_buttons: dict[str, RibbonButton] = {}
        tools = [
            ("move", "이동", "move", None),
            ("select", "선택", "select", self._menu([
                ("사각형 선택", "select", lambda: self.set_tool("select")),
                ("모두 선택", None, self.select_all),
                ("선택 해제", None, self.deselect),
                None,
                ("선택 영역 자르기", "crop", self.crop),
            ])),
            ("draw", "그리기", "draw", self._menu([
                ("연필", "draw", lambda: self._set_draw("pen")),
                ("형광펜", "highlighter", lambda: self._set_draw("highlighter")),
                ("지우개", "eraser", lambda: self._set_draw("eraser")),
            ])),
            ("fill", "채우기", "fill", self._menu([
                ("채우기 허용 오차...", None, self._fill_tolerance),
            ])),
            ("text", "텍스트", "text", self._menu([
                ("텍스트 입력", "text", lambda: self.set_tool("text")),
                ("글꼴 선택...", None, self._pick_font),
            ])),
            ("stamp", "스탬프", "stamp", self._menu([
                ("번호 스탬프", "number_stamp", lambda: self._set_stamp("number")),
                ("커서 스탬프", "cursor_stamp", lambda: self._set_stamp("cursor")),
                None,
                ("번호 초기화", None, self._reset_stamp),
                ("스탬프 크기...", None, self._stamp_size),
            ])),
            ("shape", "도형", "shape", self._shape_menu()),
        ]
        for key, label, ic, menu in tools:
            btn = RibbonButton(label, ic, menu=menu, checkable=True)
            btn.clicked.connect(lambda _=False, k=key: self.set_tool(k))
            g.add(btn)
            self.tool_buttons[key] = btn

        g = home.add_group(RibbonGroup("크기"))
        width_menu = QMenu(self)
        self.width_group = QActionGroup(self)
        for w in (1, 2, 3, 5, 8, 12, 16):
            a = width_menu.addAction(f"{w} px")
            a.setCheckable(True)
            a.setChecked(w == self.state.width)
            self.width_group.addAction(a)
            a.triggered.connect(lambda _=False, v=w: self._set_width(v))
        self.btn_width = RibbonButton(f"{self.state.width}px", "line_width", menu=width_menu)
        g.add(self.btn_width)

        g = home.add_group(RibbonGroup("색상"))
        self.sw1 = ColorSwatch("색1", self.state.color1)
        self.sw1.setChecked(True)
        self.sw1.clicked.connect(lambda: self._swatch_clicked(True))
        self.sw2 = ColorSwatch("색2", self.state.color2)
        self.sw2.clicked.connect(lambda: self._swatch_clicked(False))
        g.add(self.sw1)
        g.add(self.sw2)
        b_swap = RibbonButton("", "swap", large=False)
        b_swap.setToolTip("색1/색2 바꾸기")
        b_swap.clicked.connect(self._swap_colors)
        b_reset = RibbonButton("", "reset_colors", large=False)
        b_reset.setToolTip("기본 색상")
        b_reset.clicked.connect(self._reset_colors)
        b_pick = RibbonButton("", "eyedropper", large=False, checkable=True)
        b_pick.setToolTip("스포이트 (캔버스에서 색 추출)")
        b_pick.clicked.connect(lambda: self.set_tool("eyedropper"))
        self.tool_buttons["eyedropper"] = b_pick
        g.add_column(b_reset, b_swap, b_pick)

        g = home.add_group(RibbonGroup("팔레트"))
        self.palette = PaletteGrid(self.config.get("palette", "custom", []))
        self.palette.picked.connect(self._palette_picked)
        g.add(self.palette)
        more = RibbonButton("더보기", "more_colors")
        more.clicked.connect(self._more_colors)
        g.add(more)
        rb.add_page("홈", home)

        # 공유 -----------------------------------------------------------
        share = RibbonPage()
        g = share.add_group(RibbonGroup("저장"))
        for text, ic, slot in (("저장", "save", self.save), ("다른 이름으로", "save_as", self.save_as), ("인쇄", "print", self.print_image)):
            b = RibbonButton(text, ic)
            b.clicked.connect(slot)
            g.add(b)
        g = share.add_group(RibbonGroup("보내기"))
        for text, ic, key in (
            ("클립보드", "copy", "clipboard"),
            ("이메일", "email", "email"),
            ("FTP", "ftp", "ftp"),
            ("프로그램", "program", "program"),
            ("기본 앱", "open", "default_app"),
        ):
            b = RibbonButton(text, ic)
            b.clicked.connect(lambda _=False, k=key: self.share(k))
            g.add(b)
        rb.add_page("공유", share)

        # 보기 -----------------------------------------------------------
        view = RibbonPage()
        g = view.add_group(RibbonGroup("확대/축소"))
        for a in (self.a_zoom_in, self.a_zoom_out, self.a_zoom_100, self.a_zoom_fit):
            b = RibbonButton(a.text(), a.icon())
            b.clicked.connect(a.trigger)
            g.add(b)
        g = view.add_group(RibbonGroup("표시"))
        b_bg = RibbonButton("배경", "grid", menu=self._menu([
            ("단색 배경", None, lambda: self._set_bg_mode("solid")),
            ("격자 배경", "grid", lambda: self._set_bg_mode("grid")),
        ]))
        g.add(b_bg)
        b_status = RibbonButton("상태 표시줄", "thumbnail", checkable=True)
        b_status.setChecked(True)
        b_status.toggled.connect(self._toggle_status)
        g.add(b_status)
        g = view.add_group(RibbonGroup("개체"))
        b_merge = RibbonButton("개체 병합", "thumbnail")
        b_merge.clicked.connect(self.merge_objects)
        g.add(b_merge)
        g = view.add_group(RibbonGroup("화면 캡처"))
        cap_menu = self._menu([(label, ic, (lambda m=mode: self.ctl.capture(m))) for mode, ic, label in self._capture_entries()])
        g.add(RibbonButton("캡처", "cap_region", menu=cap_menu))
        tool_menu = self._menu([(label, ic, (lambda n=name: self.ctl.open_tool(n))) for name, ic, label in self._tool_entries()])
        g.add(RibbonButton("그래픽 도구", "tool_color_picker", menu=tool_menu))
        rb.add_page("보기", view)

        rb.file_clicked.connect(lambda: self.show_backstage("start"))
        self._doc_ribbon_keep = [heart]
        for page_idx in (2,):
            page = rb.stack.widget(page_idx)
            self._doc_ribbon_keep.extend(page.findChildren(RibbonButton)[-2:])
        self._doc_ribbon_keep.extend([paste])

    def _capture_entries(self):
        from mikmick.capture.manager import MODE_LABELS
        from mikmick.editor.backstage import CAPTURE_ITEMS

        return [(m, ic, MODE_LABELS[m]) for m, ic in CAPTURE_ITEMS]

    def _tool_entries(self):
        from mikmick.editor.backstage import TOOL_ITEMS

        return [(n, ic, t) for n, ic, t, _d in TOOL_ITEMS]

    def _shape_menu(self) -> QMenu:
        menu = QMenu(self)
        for kind in SHAPE_KINDS:
            a = menu.addAction(icon(kind), SHAPE_LABELS[kind])
            a.triggered.connect(lambda _=False, k=kind: self._set_shape(k))
        menu.addSeparator()
        self.a_shape_fill = menu.addAction("도형 채우기 (색2)")
        self.a_shape_fill.setCheckable(True)
        self.a_shape_fill.toggled.connect(lambda on: setattr(self.state, "shape_fill", on))
        return menu

    def _toggle_status(self, on: bool) -> None:
        self._status_visible = on
        self.statusBar().setVisible(on)

    def _build_status(self) -> None:
        self._status_visible = True
        sb = QStatusBar()
        self.setStatusBar(sb)
        self.lbl_size = QLabel()
        self.lbl_pos = QLabel()
        self.lbl_sel = QLabel()
        sb.addWidget(self.lbl_size)
        sb.addWidget(self.lbl_pos)
        sb.addWidget(self.lbl_sel)
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setRange(5, 800)
        self.zoom_slider.setValue(100)
        self.zoom_slider.setFixedWidth(140)
        self.zoom_slider.valueChanged.connect(lambda v: self._set_zoom(v / 100.0, from_slider=True))
        self.lbl_zoom = QLabel("100%")
        self.lbl_zoom.setMinimumWidth(48)
        sb.addPermanentWidget(self.zoom_slider)
        sb.addPermanentWidget(self.lbl_zoom)

    def apply_settings(self) -> None:
        ed = self.config.section("editor")
        pal = self.empty.palette()
        pal.setColor(self.empty.backgroundRole(), QColor(ed.get("bg_color", "#dcdcdc")))
        self.empty.setPalette(pal)
        for i in range(self.tabs.count()):
            view = self.tabs.widget(i)
            view.setBackgroundBrush(self._bg_brush())
            view.setAlignment(
                Qt.AlignmentFlag.AlignCenter if ed.get("center_image") else Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop
            )
        self.palette.set_custom(self.config.get("palette", "custom", []))

    @property
    def auto_merge_on_select(self) -> bool:
        return bool(self.config.get("editor", "auto_shape_selection", False))

    def _bg_brush(self) -> QBrush:
        if self.config.get("editor", "bg_mode", "solid") == "grid":
            return _grid_brush()
        return QBrush(QColor(self.config.get("editor", "bg_color", "#dcdcdc")))

    # ------------------------------------------------------------------ 문서
    def view(self) -> CanvasView | None:
        w = self.tabs.currentWidget()
        return w if isinstance(w, CanvasView) else None

    def doc(self) -> Document | None:
        v = self.view()
        return v.doc if v else None

    def views(self) -> list[CanvasView]:
        return [self.tabs.widget(i) for i in range(self.tabs.count())]

    def add_image(self, image: QImage, path: str | None = None, title: str | None = None) -> Document:
        if title is None and path is None:
            title = f"이미지 {self.tabs.count() + 1}"
        doc = Document(image, path, title)
        view = CanvasView(
            doc, self.state, self._next_stamp, self._bg_brush(), bool(self.config.get("editor", "center_image", False))
        )
        view.cursor_moved.connect(lambda p: self.lbl_pos.setText(f"  {p.x()}, {p.y()} px"))
        view.zoom_changed.connect(self._on_zoom_changed)
        view.color_picked.connect(self._on_color_picked)
        view.tool_finished.connect(self._sync_tool_buttons)
        doc.changed.connect(self._update_ui)
        doc.selection_changed.connect(self._update_ui)
        doc.undo.cleanChanged.connect(self._on_clean_changed)
        idx = self.tabs.addTab(view, doc.title)
        self.tabs.setCurrentIndex(idx)
        self.hide_backstage()
        QTimer.singleShot(0, lambda: self._initial_zoom(view))
        self._update_ui()
        return doc

    def _initial_zoom(self, view: CanvasView) -> None:
        vp = view.viewport().size()
        r = view.doc.scene.sceneRect()
        if r.width() > vp.width() or r.height() > vp.height():
            view.fit()
        else:
            view.set_zoom(1.0)

    def open_path(self, path: str) -> bool:
        reader = QImageReader(path)
        reader.setAutoTransform(True)
        img = reader.read()
        if img.isNull():
            QMessageBox.warning(self, APP_NAME_KO, f"이미지를 열 수 없습니다.\n{path}\n{reader.errorString()}")
            return False
        doc = self.add_image(img, os.path.abspath(path))
        doc.undo.setClean()
        self.config.add_recent_file(os.path.abspath(path))
        self.config.save()
        return True

    def open_files(self) -> None:
        formats = " ".join(f"*.{bytes(f).decode()}" for f in QImageReader.supportedImageFormats())
        start = self.config.get("recent", "files", [])
        folder = os.path.dirname(start[0]) if start else str(pictures_dir())
        paths, _ = QFileDialog.getOpenFileNames(self, "열기", folder, f"이미지 파일 ({formats});;모든 파일 (*)")
        for p in paths:
            self.open_path(p)

    def close_document(self, index: int) -> bool:
        if index < 0:
            return True
        view = self.tabs.widget(index)
        if not isinstance(view, CanvasView):
            return True
        if view.doc.modified and not self.config.get("editor", "no_save_prompt", False):
            self.tabs.setCurrentIndex(index)
            ans = QMessageBox.question(
                self,
                APP_NAME_KO,
                f"'{view.doc.title}' 의 변경 내용을 저장하시겠습니까?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel,
            )
            if ans == QMessageBox.StandardButton.Cancel:
                return False
            if ans == QMessageBox.StandardButton.Save and not self.save():
                return False
        self.tabs.removeTab(index)
        view.deleteLater()
        self._update_ui()
        return True

    def close_all(self) -> bool:
        while self.tabs.count():
            if not self.close_document(self.tabs.count() - 1):
                return False
        return True

    def _save_to(self, doc: Document, path: str) -> bool:
        from mikmick.outputs import save_image

        if not ext_of(path):
            path += "." + self.config.get("filename", "format", "png")
        if not save_image(doc.flatten(), path, self.config):
            QMessageBox.warning(self, APP_NAME_KO, f"저장하지 못했습니다.\n{path}")
            return False
        doc.path = path
        doc.title = os.path.basename(path)
        doc.undo.setClean()
        self.config.add_recent_file(path)
        self.config.save()
        self._update_titles()
        self.statusBar().showMessage(f"저장됨: {path}", 4000)
        return True

    def save(self) -> bool:
        doc = self.doc()
        if doc is None:
            return False
        if doc.path and ext_of(doc.path) in {f for f, _ in IMAGE_FORMATS}:
            return self._save_to(doc, doc.path)
        return self.save_as()

    def save_as(self) -> bool:
        doc = self.doc()
        if doc is None:
            return False
        from mikmick.outputs import next_filename

        fmt = self.config.get("filename", "format", "png")
        filters = ";;".join(label for _f, label in IMAGE_FORMATS)
        selected = next((label for f, label in IMAGE_FORMATS if f == fmt), IMAGE_FORMATS[0][1])
        if doc.path:
            suggestion = doc.path
        else:
            name, ext = next_filename(self.config)
            suggestion = os.path.join(self.config.get("autosave", "folder", "") or str(pictures_dir()), f"{name}.{ext}")
        path, chosen = QFileDialog.getSaveFileName(self, "다른 이름으로 저장", suggestion, filters, selected)
        if not path:
            return False
        if not ext_of(path):
            for f, label in IMAGE_FORMATS:
                if label == chosen:
                    path += "." + f
                    break
        return self._save_to(doc, path)

    def print_image(self) -> None:
        doc = self.doc()
        if doc is None:
            return
        try:
            from PySide6.QtPrintSupport import QPrintDialog, QPrinter
        except ImportError:
            QMessageBox.warning(self, APP_NAME_KO, "인쇄 모듈(QtPrintSupport)을 사용할 수 없습니다.")
            return
        printer = QPrinter(QPrinter.PrinterMode.HighResolution)
        dlg = QPrintDialog(printer, self)
        if dlg.exec() != QPrintDialog.DialogCode.Accepted:
            return
        img = doc.flatten()
        painter = QPainter(printer)
        rect = painter.viewport()
        size = img.size()
        size.scale(rect.size(), Qt.AspectRatioMode.KeepAspectRatio)
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height())
        painter.setWindow(img.rect())
        painter.drawImage(0, 0, img)
        painter.end()

    # ------------------------------------------------------------------ 편집
    def undo(self) -> None:
        if (d := self.doc()) is not None:
            d.undo.undo()

    def redo(self) -> None:
        if (d := self.doc()) is not None:
            d.undo.redo()

    def copy(self) -> None:
        if (d := self.doc()) is not None:
            QGuiApplication.clipboard().setImage(d.selection_image())
            self.statusBar().showMessage("클립보드에 복사했습니다.", 2500)

    def cut(self) -> None:
        d = self.doc()
        if d is None:
            return
        self.copy()
        if d.selection is not None:
            rect = QRect(d.selection)
            color = self.state.color2
            d.apply_raster(lambda img: effects.fill_rect(img, rect, color), "잘라내기")

    def delete(self) -> None:
        d = self.doc()
        if d is None:
            return
        items = d.scene.selectedItems()
        if items:
            d.remove_items(items)
        elif d.selection is not None:
            rect = QRect(d.selection)
            color = self.state.color2
            d.apply_raster(lambda img: effects.fill_rect(img, rect, color), "삭제")

    def paste(self) -> None:
        img = QGuiApplication.clipboard().image()
        if img.isNull():
            self.statusBar().showMessage("클립보드에 이미지가 없습니다.", 2500)
            return
        v = self.view()
        if v is None:
            self.add_image(img, title="클립보드")
        else:
            v.paste_pixmap(QPixmap.fromImage(img))

    def paste_as_new(self) -> None:
        img = QGuiApplication.clipboard().image()
        if img.isNull():
            self.statusBar().showMessage("클립보드에 이미지가 없습니다.", 2500)
            return
        self.add_image(img, title="클립보드")

    def select_all(self) -> None:
        if (d := self.doc()) is not None:
            d.set_selection(d.image.rect())

    def deselect(self) -> None:
        if (d := self.doc()) is not None:
            d.set_selection(None)
            d.scene.clearSelection()

    def crop(self) -> None:
        d = self.doc()
        if d is None:
            return
        if not d.crop_selection():
            self.statusBar().showMessage("먼저 '선택' 도구로 자를 영역을 지정하세요.", 3000)

    def merge_objects(self) -> None:
        if (d := self.doc()) is not None:
            d.merge_objects()

    # ------------------------------------------------------------------ 효과
    def _fx(self, name: str, fn) -> None:
        d = self.doc()
        if d is None:
            return
        rect = QRect(d.selection) if d.selection is not None else None
        QGuiApplication.setOverrideCursor(Qt.CursorShape.WaitCursor)
        try:
            d.apply_raster(lambda img: fn(img, rect), name)
        finally:
            QGuiApplication.restoreOverrideCursor()

    def _transform(self, name: str, fn) -> None:
        d = self.doc()
        if d is None:
            return
        d.set_selection(None)
        d.apply_raster(fn, name)

    def fx_blur(self) -> None:
        v = ask("흐리게", [("radius", "강도", "int", 4, (1, 40))], self)
        if v:
            self._fx("흐리게", lambda img, r: effects.blur(img, v["radius"], r))

    def fx_mosaic(self) -> None:
        v = ask("모자이크", [("block", "블록 크기", "int", 10, (2, 80))], self)
        if v:
            self._fx("모자이크", lambda img, r: effects.mosaic(img, v["block"], r))

    def fx_brightness(self) -> None:
        v = ask("밝기/대비", [("b", "밝기", "int", 0, (-100, 100)), ("c", "대비", "int", 0, (-100, 100))], self)
        if v:
            self._fx("밝기/대비", lambda img, r: effects.brightness_contrast(img, v["b"], v["c"], r))

    def fx_hue(self) -> None:
        v = ask(
            "색조/채도",
            [("h", "색조", "int", 0, (-180, 180)), ("s", "채도", "int", 0, (-100, 100)), ("l", "밝기", "int", 0, (-100, 100))],
            self,
        )
        if v:
            self._fx("색조/채도", lambda img, r: effects.hue_saturation(img, v["h"], v["s"], v["l"], r))

    def fx_border(self) -> None:
        v = ask(
            "테두리",
            [
                ("w", "두께", "int", 4, (1, 100)),
                ("color", "색상", "color", self.state.color1, None),
                ("inside", "이미지 안쪽에 그리기", "bool", False, None),
            ],
            self,
        )
        if v:
            self._transform("테두리", lambda img: effects.border(img, v["w"], v["color"], v["inside"]))

    def fx_shadow(self) -> None:
        self._transform("그림자", lambda img: effects.drop_shadow(img))

    def fx_watermark(self) -> None:
        v = ask(
            "워터마크",
            [
                ("text", "텍스트", "text", "MikMick", None),
                ("size", "글자 크기", "int", 28, (8, 200)),
                ("opacity", "불투명도", "float", 0.35, (0.05, 1.0)),
                ("color", "색상", "color", QColor("white"), None),
                (
                    "pos",
                    "위치",
                    "choice",
                    "bottom-right",
                    [
                        ("bottom-right", "오른쪽 아래"),
                        ("bottom-left", "왼쪽 아래"),
                        ("top-right", "오른쪽 위"),
                        ("top-left", "왼쪽 위"),
                        ("center", "가운데"),
                        ("tile", "바둑판 배열"),
                    ],
                ),
            ],
            self,
        )
        if v and v["text"]:
            self._transform(
                "워터마크", lambda img: effects.watermark(img, v["text"], v["color"], v["opacity"], v["pos"], v["size"])
            )

    def resize_image(self) -> None:
        d = self.doc()
        if d is None:
            return
        dlg = ResizeDialog(d.size, parent=self)
        if dlg.exec():
            s = dlg.result_size()
            self._transform("크기 조절", lambda img: effects.resize(img, s.width(), s.height()))

    def resize_canvas(self) -> None:
        d = self.doc()
        if d is None:
            return
        dlg = ResizeDialog(d.size, "캔버스 크기 조절", self, with_anchor=True)
        if dlg.exec():
            s = dlg.result_size()
            anchor = dlg.anchor.currentData()
            fill = dlg.fill.color
            self._transform("캔버스 크기", lambda img: effects.canvas_size(img, s.width(), s.height(), anchor, fill))

    # ------------------------------------------------------------------ 공유
    def share(self, key: str) -> None:
        d = self.doc()
        if d is None:
            return
        img = d.flatten()
        self.ctl.share_image(img, key, self, d.path)

    # ------------------------------------------------------------------ 도구 상태
    def set_tool(self, tool: str) -> None:
        self.state.tool = tool
        for v in self.views():
            v.apply_tool()
        self._sync_tool_buttons()

    def _sync_tool_buttons(self) -> None:
        for key, btn in self.tool_buttons.items():
            btn.setChecked(key == self.state.tool)

    def _set_draw(self, kind: str) -> None:
        self.state.draw_kind = kind
        self.tool_buttons["draw"].setIcon(icon({"pen": "draw", "highlighter": "highlighter", "eraser": "eraser"}[kind]))
        self.set_tool("draw")

    def _set_shape(self, kind: str) -> None:
        self.state.shape_kind = kind
        self.tool_buttons["shape"].setIcon(icon(kind))
        self.set_tool("shape")

    def _set_stamp(self, kind: str) -> None:
        self.state.stamp_kind = kind
        self.tool_buttons["stamp"].setIcon(icon("number_stamp" if kind == "number" else "cursor_stamp"))
        self.set_tool("stamp")

    def _next_stamp(self) -> int:
        self._stamp_no += 1
        return self._stamp_no

    def _reset_stamp(self) -> None:
        self._stamp_no = 0
        self.statusBar().showMessage("스탬프 번호를 1부터 다시 시작합니다.", 2500)

    def _stamp_size(self) -> None:
        v = ask("스탬프 크기", [("s", "크기", "int", self.state.stamp_size, (12, 120))], self)
        if v:
            self.state.stamp_size = v["s"]

    def _fill_tolerance(self) -> None:
        v = ask("채우기 허용 오차", [("t", "허용 오차", "int", self.state.fill_tolerance, (0, 255))], self)
        if v:
            self.state.fill_tolerance = v["t"]
            self.set_tool("fill")

    def _pick_font(self) -> None:
        first, second = QFontDialog.getFont(self.state.font, self, "글꼴")
        ok, font = (first, second) if isinstance(first, bool) else (second, first)
        if ok:
            self.state.font = font
            for item in self.doc().scene.selectedItems() if self.doc() else []:
                if hasattr(item, "setFont"):
                    item.setFont(font)
            self.set_tool("text")

    def _set_width(self, w: int) -> None:
        self.state.width = w
        self.btn_width.setText(f"{w}px")

    def _swatch_clicked(self, first: bool) -> None:
        self.sw1.setChecked(first)
        self.sw2.setChecked(not first)
        current = self.state.color1 if first else self.state.color2
        c = QColorDialog.getColor(current, self, "색1" if first else "색2", QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if c.isValid():
            self._set_color(c, first)

    def _set_color(self, c: QColor, first: bool) -> None:
        if first:
            self.state.color1 = QColor(c)
            self.sw1.set_color(c)
        else:
            self.state.color2 = QColor(c)
            self.sw2.set_color(c)
        d = self.doc()
        if d is not None and first:
            for item in d.scene.selectedItems():
                if hasattr(item, "color"):
                    item.color = QColor(c)
                    item.update()
                elif hasattr(item, "setDefaultTextColor"):
                    item.setDefaultTextColor(c)

    def _palette_picked(self, c: QColor, primary: bool) -> None:
        first = primary if not self.sw2.isChecked() else not primary
        self._set_color(c, first)

    def _on_color_picked(self, c: QColor, primary: bool) -> None:
        self._set_color(c, primary)

    def _swap_colors(self) -> None:
        c1, c2 = QColor(self.state.color1), QColor(self.state.color2)
        self._set_color(c2, True)
        self._set_color(c1, False)

    def _reset_colors(self) -> None:
        self._set_color(QColor("black"), True)
        self._set_color(QColor("white"), False)

    def _more_colors(self) -> None:
        c = QColorDialog.getColor(self.state.color1, self, "색 편집", QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if not c.isValid():
            return
        custom = [x for x in self.config.get("palette", "custom", []) if x != c.name()]
        custom.insert(0, c.name())
        self.config.set("palette", "custom", custom[:10])
        self.config.save()
        self.palette.set_custom(custom[:10])
        self._set_color(c, True)

    def _set_bg_mode(self, mode: str) -> None:
        self.config.set("editor", "bg_mode", mode)
        self.config.save()
        self.apply_settings()

    # ------------------------------------------------------------------ 확대/축소
    def _set_zoom(self, z: float, from_slider: bool = False) -> None:
        v = self.view()
        if v is not None:
            v.set_zoom(z)

    def _zoom_by(self, f: float) -> None:
        v = self.view()
        if v is not None:
            v.set_zoom(v.zoom * f)

    def _fit(self) -> None:
        v = self.view()
        if v is not None:
            v.fit()

    def _on_zoom_changed(self, z: float) -> None:
        self.lbl_zoom.setText(f"{round(z * 100)}%")
        self.zoom_slider.blockSignals(True)
        self.zoom_slider.setValue(round(z * 100))
        self.zoom_slider.blockSignals(False)

    # ------------------------------------------------------------------ 백스테이지
    def show_backstage(self, page: str = "start") -> None:
        self.backstage.set_has_document(self.doc() is not None)
        self.backstage.show_page(page)
        if page == "thumbnail":
            self._refresh_thumbnails()
        self.stack.setCurrentWidget(self.backstage)
        self.statusBar().hide()

    def hide_backstage(self) -> None:
        self.stack.setCurrentIndex(0)
        self.statusBar().setVisible(self._status_visible)

    def _refresh_thumbnails(self) -> None:
        docs = [(v.doc.title, QPixmap.fromImage(v.doc.flatten())) for v in self.views()]
        self.backstage.thumb_page.set_documents(docs)

    def _on_hide_start(self, hide: bool) -> None:
        self.config.set("general", "hide_start_page", hide)
        self.config.save()

    def _on_backstage(self, kind: str, name: str) -> None:
        if kind == "create":
            size_s, color_s = name.split(":", 1)
            w, h = (int(x) for x in size_s.split("x"))
            color = QColor(color_s)
            img = QImage(w, h, QImage.Format.Format_ARGB32)
            img.fill(color)
            ni = self.config.section("new_image")
            ni.update({"preset": self.backstage.new_page.preset_key(), "width": w, "height": h, "bg": color.name(QColor.NameFormat.HexArgb)})
            self.config.save()
            if self.backstage.new_page.preset_key() == "clipboard":
                clip = QGuiApplication.clipboard().image()
                if not clip.isNull() and clip.size() == QSize(w, h):
                    p = QPainter(img)
                    p.drawImage(0, 0, clip)
                    p.end()
            self.add_image(img)
        elif kind == "open":
            self.hide_backstage()
            self.open_files()
        elif kind == "save":
            self.hide_backstage()
            self.save()
        elif kind == "save_as":
            self.hide_backstage()
            self.save_as()
        elif kind == "print":
            self.hide_backstage()
            self.print_image()
        elif kind == "close":
            self.close_document(self.tabs.currentIndex())
            if self.tabs.count() == 0:
                self.show_backstage("start")
            else:
                self.hide_backstage()
        elif kind == "share":
            self.share(name)
        elif kind == "thumbnails":
            self._refresh_thumbnails()
        elif kind == "activate":
            self.tabs.setCurrentIndex(int(name))
            self.hide_backstage()
        elif kind == "options":
            self.ctl.show_options(self)
        elif kind == "capture":
            self.ctl.capture(name)
        elif kind == "tool":
            self.ctl.open_tool(name)
        elif kind == "homepage":
            self.ctl.homepage()
        elif kind == "check_update":
            self.ctl.check_updates(manual=True, parent=self)

    # ------------------------------------------------------------------ 상태 갱신
    def _on_tab_changed(self, _i: int) -> None:
        v = self.view()
        if v is not None:
            self._on_zoom_changed(v.zoom)
            v.apply_tool()
        self._update_ui()

    def _on_clean_changed(self, _clean: bool) -> None:
        self._update_titles()

    def _update_titles(self) -> None:
        for i, v in enumerate(self.views()):
            self.tabs.setTabText(i, ("*" if not v.doc.undo.isClean() else "") + v.doc.title)
        d = self.doc()
        self.setWindowTitle(f"{d.title} - {APP_NAME_KO}" if d else APP_NAME_KO)

    def _update_ui(self) -> None:
        d = self.doc()
        has = d is not None
        self.canvas_stack.setCurrentIndex(1 if has else 0)
        for a in self.doc_actions:
            a.setEnabled(has)
        self.a_undo.setEnabled(has and d.undo.canUndo())
        self.a_redo.setEnabled(has and d.undo.canRedo())
        self.ribbon.set_body_enabled(has, self._doc_ribbon_keep)
        self.zoom_slider.setEnabled(has)
        if has:
            self.lbl_size.setText(f"  {d.size.width()} x {d.size.height()} px")
            sel = d.selection
            self.lbl_sel.setText(f"  선택: {sel.width()} x {sel.height()}" if sel is not None else "")
        else:
            self.lbl_size.setText("")
            self.lbl_pos.setText("")
            self.lbl_sel.setText("")
        self._update_titles()
        self._sync_tool_buttons()

    # ------------------------------------------------------------------ 창 이벤트
    def dragEnterEvent(self, event: QDragEnterEvent) -> None:
        if event.mimeData().hasUrls() or event.mimeData().hasImage():
            event.acceptProposedAction()

    def dropEvent(self, event: QDropEvent) -> None:
        md = event.mimeData()
        if md.hasUrls():
            for url in md.urls():
                if url.isLocalFile():
                    self.open_path(url.toLocalFile())
        elif md.hasImage():
            self.add_image(QImage(md.imageData()), title="드롭한 이미지")

    def closeEvent(self, event: QCloseEvent) -> None:
        if self.config.get("editor", "quit_on_close", False) or not self.ctl.tray_available():
            if self.close_all():
                event.accept()
                self.ctl.quit()
            else:
                event.ignore()
            return
        if not self.close_all():
            event.ignore()
            return
        event.ignore()
        self.hide()

    def bring_to_front(self) -> None:
        if self.isMinimized():
            self.showNormal()
        self.show()
        self.raise_()
        self.activateWindow()

    def center_on_screen(self) -> None:
        screen = QGuiApplication.screenAt(QPoint(self.x() + 10, self.y() + 10)) or QGuiApplication.primaryScreen()
        if screen is None:
            return
        g = screen.availableGeometry()
        self.move(g.center() - self.rect().center())
