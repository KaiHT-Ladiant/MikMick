"""믹믹 애플리케이션 진입점 및 컨트롤러."""

from __future__ import annotations

import argparse
import getpass
import json
import os
import sys
import threading

from PySide6.QtCore import QObject, QPoint, QTimer, QUrl, Signal
from PySide6.QtGui import QCursor, QDesktopServices, QGuiApplication, QIcon, QImage, QPixmap
from PySide6.QtNetwork import QLocalServer, QLocalSocket
from PySide6.QtWidgets import QApplication, QFileDialog, QMenu, QMessageBox, QSystemTrayIcon, QWidget

from mikmick import APP_ID, APP_NAME, APP_NAME_KO, REPO_URL, __version__
from mikmick.capture import backend
from mikmick.capture.manager import MODE_LABELS, CaptureManager
from mikmick.config import CAPTURE_MODES, TOOL_NAMES, Config, pictures_dir
from mikmick.hotkeys import HotkeyManager
from mikmick.icons import icon

SERVER_NAME = f"mikmick-{getpass.getuser()}"

TOOL_LABELS = {
    "editor": "믹믹 에디터",
    "color_picker": "색상 추출 도구",
    "palette": "색상 팔레트",
    "magnifier": "돋보기",
    "ruler": "눈금자",
    "crosshair": "십자선",
    "protractor": "각도기",
    "whiteboard": "프리젠테이션 도구",
}


class _UpdateSignal(QObject):
    done = Signal(object, bool, object)


class Controller(QObject):
    def __init__(self, app: QApplication, config: Config):
        super().__init__()
        self.app = app
        self.config = config
        self._editor = None
        self._hidden_for_capture: list[QWidget] = []
        self._tools: list[QWidget] = []
        self.capture_manager = CaptureManager(config, self._hide_for_capture, self._restore_after_capture)
        self.capture_manager.captured.connect(self.on_captured)
        self.capture_manager.failed.connect(self._show_error)
        self.hotkeys = HotkeyManager()
        self.hotkeys.triggered.connect(self.dispatch_action)
        self.hotkeys.apply(config.section("hotkeys"))
        self._update_signal = _UpdateSignal()
        self._update_signal.done.connect(self._on_update_result)
        self.tray: QSystemTrayIcon | None = None
        self._build_tray()

    # ------------------------------------------------------------------ 에디터
    @property
    def editor(self):
        if self._editor is None:
            from mikmick.editor.window import EditorWindow

            self._editor = EditorWindow(self)
        return self._editor

    def show_editor(self, start_page: bool | None = None) -> None:
        ed = self.editor
        if start_page is None:
            start_page = not self.config.get("general", "hide_start_page", False) and ed.doc() is None
        if start_page:
            ed.show_backstage("start")
        ed.bring_to_front()

    # ------------------------------------------------------------------ 트레이
    def tray_available(self) -> bool:
        return self.tray is not None and QSystemTrayIcon.isSystemTrayAvailable()

    def _build_tray(self) -> None:
        if not QSystemTrayIcon.isSystemTrayAvailable():
            return
        tray = QSystemTrayIcon(icon("logo"), self)
        tray.setToolTip(f"{APP_NAME_KO} {__version__}")
        menu = QMenu()
        menu.addAction(icon("tool_editor"), "믹믹 에디터", lambda: self.show_editor())
        menu.addSeparator()
        from mikmick.editor.backstage import CAPTURE_ITEMS, TOOL_ITEMS

        for mode, ic in CAPTURE_ITEMS:
            menu.addAction(icon(ic), MODE_LABELS[mode], lambda m=mode: self.capture(m))
        delay_menu = menu.addMenu(icon("cap_delay"), "지연 캡처 (영역 지정)")
        for sec in (3, 5, 10):
            delay_menu.addAction(f"{sec}초 후", lambda s=sec: self.capture("region", s * 1000))
        menu.addSeparator()
        tools = menu.addMenu(icon("tool_color_picker"), "그래픽 도구")
        for name, ic, title, _desc in TOOL_ITEMS:
            tools.addAction(icon(ic), title, lambda n=name: self.open_tool(n))
        result_menu = menu.addMenu("캡처 결과")
        from mikmick.options import RESULT_CHOICES

        self._result_actions = []
        for key, label in RESULT_CHOICES:
            a = result_menu.addAction(label)
            a.setCheckable(True)
            a.triggered.connect(lambda _=False, k=key: self._set_result(k))
            self._result_actions.append((key, a))
        result_menu.aboutToShow.connect(self._sync_result_menu)
        menu.addSeparator()
        menu.addAction(icon("options"), "옵션", lambda: self.show_options(None))
        menu.addAction(icon("info"), "정보", lambda: (self.show_editor(False), self.editor.show_backstage("info")))
        menu.addSeparator()
        menu.addAction("종료", self.quit)
        tray.setContextMenu(menu)
        tray.activated.connect(self._tray_activated)
        tray.show()
        self._tray_menu = menu
        self.tray = tray

    def _sync_result_menu(self) -> None:
        current = self.config.get("capture", "result", "editor")
        for key, a in self._result_actions:
            a.setChecked(key == current)

    def _set_result(self, key: str) -> None:
        self.config.set("capture", "result", key)
        self.config.save()

    def _tray_activated(self, reason: QSystemTrayIcon.ActivationReason) -> None:
        if reason in (QSystemTrayIcon.ActivationReason.Trigger, QSystemTrayIcon.ActivationReason.DoubleClick):
            self.show_editor()

    def notify(self, message: str, title: str = APP_NAME_KO) -> None:
        if self.tray is not None and self.tray.supportsMessages():
            self.tray.showMessage(title, message, icon("logo"), 3000)
        elif self._editor is not None and self._editor.isVisible():
            self._editor.statusBar().showMessage(message, 4000)

    # ------------------------------------------------------------------ 캡처
    def capture(self, mode: str, delay_ms: int | None = None) -> None:
        if mode not in CAPTURE_MODES:
            return
        self.capture_manager.capture(mode, delay_ms)

    def _hide_for_capture(self) -> bool:
        self._hidden_for_capture = []
        if self._editor is not None and self._editor.isVisible() and self.config.get("editor", "hide_on_capture", True):
            self._hidden_for_capture.append(self._editor)
        for w in self._tools:
            try:
                if w.isVisible():
                    self._hidden_for_capture.append(w)
            except RuntimeError:
                continue
        for w in self._hidden_for_capture:
            w.hide()
        return bool(self._hidden_for_capture)

    def _restore_after_capture(self) -> None:
        for w in self._hidden_for_capture:
            try:
                if w is not self._editor:
                    w.show()
            except RuntimeError:
                continue
        if self._editor is not None and self._editor in self._hidden_for_capture:
            if self.config.get("capture", "result", "editor") != "editor":
                self._editor.show()
        self._hidden_for_capture = []

    def on_captured(self, pm: QPixmap, mode: str) -> None:
        from mikmick import outputs

        image = pm.toImage()
        cfg = self.config
        saved_path = None
        if cfg.get("capture", "always_clipboard", False):
            outputs.copy_to_clipboard(image)
        if cfg.get("autosave", "enabled", False):
            saved_path = outputs.autosave(image, cfg)
        result = cfg.get("capture", "result", "editor")
        if result == "editor":
            self.show_editor(False)
            self.editor.add_image(image, path=None, title=MODE_LABELS.get(mode, "캡처"))
            self.editor.bring_to_front()
        elif result == "clipboard":
            outputs.copy_to_clipboard(image)
            self.notify("캡처한 이미지를 클립보드에 복사했습니다.")
        elif result == "file":
            name, ext = outputs.next_filename(cfg)
            folder = cfg.get("autosave", "folder", "") or str(pictures_dir())
            path, _ = QFileDialog.getSaveFileName(None, "다른 이름으로 저장", os.path.join(folder, f"{name}.{ext}"))
            if path and outputs.save_image(image, path, cfg):
                self.notify(f"저장됨: {path}")
        elif result == "autosave":
            path = saved_path or outputs.autosave(image, cfg)
            if path:
                self.notify(f"자동 저장됨: {path}")
        elif result == "ftp":
            self.share_image(image, "ftp", None, saved_path)
        elif result == "program":
            self.share_image(image, "program", None, saved_path)

    # ------------------------------------------------------------------ 공유
    def share_image(self, image: QImage, key: str, parent: QWidget | None, path: str | None = None) -> None:
        from mikmick import outputs

        cfg = self.config
        if key == "clipboard":
            outputs.copy_to_clipboard(image)
            self.notify("클립보드에 복사했습니다.")
            return
        file_path = path if path and os.path.exists(path) else outputs.temp_save(image, cfg)
        if not file_path:
            self._show_error("임시 파일을 저장하지 못했습니다.", parent)
            return
        if key == "email":
            if not outputs.send_email(file_path):
                self._show_error("xdg-email 을 실행할 수 없습니다.", parent)
        elif key == "default_app":
            outputs.open_with_default(file_path)
        elif key == "program":
            if not cfg.get("autosave", "program", ""):
                self._show_error("옵션 > 자동 저장 > 외부 프로그램 연결에서 프로그램을 먼저 지정하세요.", parent)
                return
            if not outputs.run_program(file_path, cfg):
                self._show_error("외부 프로그램을 실행할 수 없습니다.", parent)
        elif key == "ftp":
            if not cfg.get("ftp", "server", ""):
                self._show_error("옵션 > FTP 설정에서 서버를 먼저 지정하세요.", parent)
                return
            try:
                url = outputs.ftp_upload(file_path, cfg)
                self.notify("FTP 전송 완료" + (f": {url}" if url else ""))
            except Exception as exc:
                self._show_error(f"FTP 전송 실패: {exc}", parent)

    # ------------------------------------------------------------------ 도구
    def _keep(self, w: QWidget) -> QWidget:
        self._tools = [t for t in self._tools if _alive(t)]
        self._tools.append(w)
        w.destroyed.connect(lambda _=None, ref=w: self._tools.remove(ref) if ref in self._tools else None)
        return w

    def open_tool(self, name: str) -> None:
        if name == "editor":
            self.show_editor()
            return
        if name not in TOOL_NAMES:
            return
        hide = self._editor is not None and self._editor.isVisible() and name in ("color_picker", "crosshair", "protractor", "whiteboard")
        if hide:
            self._editor.hide()
        QTimer.singleShot(300 if hide else 30, lambda: self._open_tool(name, hide))

    def _open_tool(self, name: str, restore_editor: bool) -> None:
        from mikmick.tools import colors, screen_tools, whiteboard

        multi = bool(self.config.get("capture", "multi_monitor", True))
        zoom = int(self.config.get("capture", "magnifier_zoom", 6))

        def restore():
            if restore_editor and self._editor is not None:
                self._editor.show()

        if name in ("color_picker", "crosshair", "protractor"):
            snap = backend.grab_screen(multi)
            if snap is None:
                restore()
                self._show_error("화면을 캡처할 수 없습니다.")
                return
            if name == "color_picker":
                overlay = colors.ColorPickOverlay(snap, max(zoom, 8))

                def picked(c):
                    restore()
                    win = self._color_window(False)
                    win.set_color(c)
                    win.bring_to_front()
                    QGuiApplication.clipboard().setText(win.code.text())
                    self.notify(f"색상 {win.code.text()} 을(를) 클립보드에 복사했습니다.")

                overlay.picked.connect(picked)
                overlay.cancelled.connect(restore)
            elif name == "crosshair":
                overlay = screen_tools.CrosshairOverlay(snap, zoom)
                overlay.cancelled.connect(restore)
            else:
                overlay = screen_tools.ProtractorOverlay(snap, zoom)
                overlay.cancelled.connect(restore)
            self._keep(overlay)
            overlay.start()
        elif name == "palette":
            self._color_window(True).bring_to_front()
        elif name == "magnifier":
            snap = backend.grab_screen(multi) if screen_tools.needs_snapshot_for_live_tools() else None
            mag = screen_tools.Magnifier(self.config, snap)
            self._keep(mag)
            pos = QCursor.pos()
            mag.move(pos + QPoint(40, 40))
            mag.show()
        elif name == "ruler":
            ruler = screen_tools.Ruler(self.config)
            self._keep(ruler)
            ruler.start()
        elif name == "whiteboard":
            snap = backend.grab_screen(multi) if backend.is_wayland() else None
            wb = whiteboard.Whiteboard(snap)
            wb.destroyed.connect(lambda _=None: restore())
            self._keep(wb)
            wb.start()

    def _color_window(self, palette_mode: bool):
        from mikmick.tools.colors import ColorToolWindow

        for w in self._tools:
            if isinstance(w, ColorToolWindow) and _alive(w):
                return w
        win = ColorToolWindow(self.config, palette_mode)
        win.pick_again.connect(lambda: (win.hide(), QTimer.singleShot(250, lambda: self._open_tool("color_picker", False))))
        self._keep(win)
        return win

    def dispatch_action(self, action: str) -> None:
        if action in CAPTURE_MODES:
            self.capture(action)
        elif action in TOOL_NAMES:
            self.open_tool(action)

    # ------------------------------------------------------------------ 옵션 / 정보
    def show_options(self, parent: QWidget | None) -> None:
        from mikmick.options import OptionsDialog
        from mikmick.system import set_autostart

        dlg = OptionsDialog(self.config, parent)
        if dlg.exec():
            self.hotkeys.apply(self.config.section("hotkeys"))
            if self.hotkeys.error and any(self.config.section("hotkeys").values()):
                self.notify(self.hotkeys.error)
            if sys.platform == "linux":
                set_autostart(
                    bool(self.config.get("general", "autostart", False)),
                    self.config.get("general", "autostart_mode", "tray") == "tray",
                )
            if self._editor is not None:
                self._editor.apply_settings()

    def homepage(self) -> None:
        QDesktopServices.openUrl(QUrl(REPO_URL))

    def check_updates(self, manual: bool = False, parent: QWidget | None = None) -> None:
        from mikmick.system import fetch_latest_release

        def work():
            self._update_signal.done.emit(fetch_latest_release(), manual, parent)

        threading.Thread(target=work, daemon=True).start()

    def _on_update_result(self, result, manual: bool, parent) -> None:
        from mikmick.system import is_newer

        if result is None:
            if manual:
                QMessageBox.information(parent, APP_NAME_KO, "업데이트 정보를 가져오지 못했습니다.\n(아직 릴리스가 없거나 네트워크에 연결되지 않았습니다.)")
            return
        tag, url = result
        if is_newer(tag):
            ans = QMessageBox.question(
                parent, APP_NAME_KO, f"새 버전 {tag} 이(가) 있습니다. (현재 {__version__})\n다운로드 페이지를 여시겠습니까?"
            )
            if ans == QMessageBox.StandardButton.Yes:
                QDesktopServices.openUrl(QUrl(url or REPO_URL + "/releases"))
        elif manual:
            QMessageBox.information(parent, APP_NAME_KO, f"최신 버전({__version__})을 사용 중입니다.")

    def _show_error(self, message: str, parent: QWidget | None = None) -> None:
        QMessageBox.warning(parent or (self._editor if self._editor and self._editor.isVisible() else None), APP_NAME_KO, message)

    def quit(self) -> None:
        if self._editor is not None and self._editor.isVisible() and not self._editor.close_all():
            return
        self.hotkeys.stop()
        if self.tray is not None:
            self.tray.hide()
        self.app.quit()

    # ------------------------------------------------------------------ IPC
    def handle_message(self, msg: dict) -> None:
        if msg.get("capture"):
            self.capture(msg["capture"], msg.get("delay"))
        elif msg.get("tool"):
            self.open_tool(msg["tool"])
        elif msg.get("options"):
            self.show_options(None)
        elif msg.get("quit"):
            self.quit()
        else:
            files = msg.get("files") or []
            if files:
                self.show_editor(False)
                for f in files:
                    self.editor.open_path(f)
            elif not msg.get("tray"):
                self.show_editor()


def _alive(w: QWidget) -> bool:
    try:
        w.objectName()
        return True
    except RuntimeError:
        return False


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="mikmick", description="믹믹(MikMick) - 리눅스용 화면 캡처 & 이미지 편집 도구")
    p.add_argument("files", nargs="*", help="에디터로 열 이미지 파일")
    p.add_argument("-c", "--capture", choices=CAPTURE_MODES, help="캡처 모드 실행")
    p.add_argument("-d", "--delay", type=int, default=None, help="캡처 지연 (ms)")
    p.add_argument("-t", "--tool", choices=TOOL_NAMES, help="그래픽 도구 실행")
    p.add_argument("--tray", action="store_true", help="알림 영역에만 표시하여 시작")
    p.add_argument("--options", action="store_true", help="옵션 대화상자 열기")
    p.add_argument("--quit", action="store_true", help="실행 중인 믹믹 종료")
    p.add_argument("-V", "--version", action="version", version=f"{APP_NAME} {__version__}")
    return p


def _send_to_running(msg: dict) -> bool:
    sock = QLocalSocket()
    sock.connectToServer(SERVER_NAME)
    if not sock.waitForConnected(400):
        return False
    sock.write(json.dumps(msg).encode("utf-8"))
    sock.flush()
    sock.waitForBytesWritten(1000)
    sock.disconnectFromServer()
    return True


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    msg = {
        "files": [os.path.abspath(f) for f in args.files],
        "capture": args.capture,
        "delay": args.delay,
        "tool": args.tool,
        "tray": args.tray,
        "options": args.options,
        "quit": args.quit,
    }

    QGuiApplication.setDesktopFileName(APP_ID)
    app = QApplication(sys.argv[:1])
    app.setApplicationName(APP_NAME)
    app.setApplicationDisplayName(APP_NAME_KO)
    app.setApplicationVersion(__version__)
    app.setOrganizationName(APP_NAME)
    app.setQuitOnLastWindowClosed(False)
    app.setWindowIcon(icon("logo") if not QIcon.hasThemeIcon("mikmick") else QIcon.fromTheme("mikmick"))

    if _send_to_running(msg):
        return 0
    if args.quit:
        return 0

    QLocalServer.removeServer(SERVER_NAME)
    server = QLocalServer()
    server.listen(SERVER_NAME)

    config = Config()
    ctl = Controller(app, config)

    def on_connection():
        sock = server.nextPendingConnection()
        if sock is None:
            return

        def read():
            data = bytes(sock.readAll()).decode("utf-8", "replace")
            try:
                ctl.handle_message(json.loads(data))
            except ValueError:
                pass

        if sock.waitForReadyRead(1000):
            read()
        sock.disconnectFromServer()

    server.newConnection.connect(on_connection)

    if args.capture or args.tool or args.options or args.files:
        QTimer.singleShot(0, lambda: ctl.handle_message(msg))
    else:
        tray_only = args.tray or config.get("general", "start_mode", "editor") == "tray"
        if tray_only and ctl.tray_available():
            ctl.notify("믹믹이 알림 영역에서 실행 중입니다.")
        else:
            ctl.show_editor()
    if config.get("general", "check_updates", True):
        QTimer.singleShot(5000, lambda: ctl.check_updates(False, None))
    if not ctl.tray_available() and (args.capture or args.tool):
        # 트레이가 없는 환경에서 CLI 로 실행한 경우, 작업이 끝나고 창이 모두 닫히면 종료한다.
        app.setQuitOnLastWindowClosed(True)

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
