"""플랫폼별 화면 획득 백엔드.

* X11 : Qt(QScreen.grabWindow) 로 루트 윈도우를 직접 캡처
* Wayland : xdg-desktop-portal Screenshot (jeepney) → grim → gnome-screenshot → spectacle 순으로 시도
"""

from __future__ import annotations

import os
import random
import shutil
import string
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from urllib.parse import unquote, urlparse

from PySide6.QtCore import QPoint, QRect
from PySide6.QtGui import QCursor, QGuiApplication, QImage, QPixmap


def is_wayland() -> bool:
    if sys.platform != "linux":
        return False
    return os.environ.get("XDG_SESSION_TYPE", "").lower() == "wayland" or bool(os.environ.get("WAYLAND_DISPLAY"))


def is_x11() -> bool:
    return sys.platform == "linux" and not is_wayland() and bool(os.environ.get("DISPLAY"))


def has_tool(name: str) -> bool:
    return shutil.which(name) is not None


@dataclass
class Snapshot:
    """한 번에 획득한 화면 이미지와 그것이 덮는 논리 좌표 영역."""

    pixmap: QPixmap
    geometry: QRect

    @property
    def scale_x(self) -> float:
        return self.pixmap.width() / max(1, self.geometry.width())

    @property
    def scale_y(self) -> float:
        return self.pixmap.height() / max(1, self.geometry.height())

    def crop(self, rect: QRect) -> QPixmap:
        """논리 좌표(rect, 전역 좌표계) 영역을 잘라낸다."""
        local = rect.translated(-self.geometry.topLeft())
        src = QRect(
            round(local.x() * self.scale_x),
            round(local.y() * self.scale_y),
            round(local.width() * self.scale_x),
            round(local.height() * self.scale_y),
        )
        out = self.pixmap.copy(src)
        out.setDevicePixelRatio(1.0)
        return out


def virtual_geometry(multi_monitor: bool = True) -> QRect:
    screens = QGuiApplication.screens()
    if not screens:
        return QRect(0, 0, 1920, 1080)
    if not multi_monitor:
        screen = QGuiApplication.screenAt(QCursor.pos()) or QGuiApplication.primaryScreen()
        return screen.geometry()
    rect = QRect()
    for s in screens:
        rect = rect.united(s.geometry())
    return rect


def _grab_qt(geometry: QRect) -> QPixmap | None:
    screen = QGuiApplication.primaryScreen()
    if screen is None:
        return None
    if sys.platform == "linux":
        pm = screen.grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height())
    else:
        # 비 리눅스 개발 환경: 화면별로 캡처해 이어 붙인다.
        image = QImage(geometry.size() * screen.devicePixelRatio(), QImage.Format.Format_ARGB32)
        image.fill(0)
        from PySide6.QtGui import QPainter

        painter = QPainter(image)
        dpr = screen.devicePixelRatio()
        for s in QGuiApplication.screens():
            g = s.geometry()
            if not g.intersects(geometry):
                continue
            part = s.grabWindow(0)
            target = QRect(
                round((g.x() - geometry.x()) * dpr),
                round((g.y() - geometry.y()) * dpr),
                round(g.width() * dpr),
                round(g.height() * dpr),
            )
            painter.drawPixmap(target, part)
        painter.end()
        pm = QPixmap.fromImage(image)
    if pm.isNull():
        return None
    pm.setDevicePixelRatio(1.0)
    return pm


def _portal_screenshot(interactive: bool = False) -> str | None:
    """xdg-desktop-portal Screenshot 호출, 저장된 파일 경로를 반환."""
    try:
        from jeepney import DBusAddress, MatchRule, new_method_call
        from jeepney.bus_messages import message_bus
        from jeepney.io.blocking import Proxy, open_dbus_connection
    except ImportError:
        return None
    try:
        conn = open_dbus_connection(bus="SESSION")
    except Exception:
        return None
    try:
        token = "mikmick_" + "".join(random.choices(string.ascii_lowercase, k=8))
        sender = conn.unique_name[1:].replace(".", "_")
        handle = f"/org/freedesktop/portal/desktop/request/{sender}/{token}"
        rule = MatchRule(
            type="signal",
            interface="org.freedesktop.portal.Request",
            member="Response",
            path=handle,
        )
        Proxy(message_bus, conn).AddMatch(rule)
        portal = DBusAddress(
            "/org/freedesktop/portal/desktop",
            bus_name="org.freedesktop.portal.Desktop",
            interface="org.freedesktop.portal.Screenshot",
        )
        with conn.filter(rule) as queue:
            msg = new_method_call(
                portal,
                "Screenshot",
                "sa{sv}",
                ("", {"handle_token": ("s", token), "interactive": ("b", interactive)}),
            )
            conn.send_and_get_reply(msg, timeout=10)
            signal = conn.recv_until_filtered(queue, timeout=120)
        response, results = signal.body
        if response != 0 or "uri" not in results:
            return None
        uri = results["uri"][1]
        return unquote(urlparse(uri).path)
    except Exception:
        return None
    finally:
        conn.close()


def _external_screenshot() -> str | None:
    fd, path = tempfile.mkstemp(prefix="mikmick-", suffix=".png")
    os.close(fd)
    commands = []
    if has_tool("grim"):
        commands.append(["grim", path])
    if has_tool("gnome-screenshot"):
        commands.append(["gnome-screenshot", "-f", path])
    if has_tool("spectacle"):
        commands.append(["spectacle", "-b", "-n", "-f", "-o", path])
    if has_tool("scrot"):
        commands.append(["scrot", "-o", path])
    if has_tool("maim"):
        commands.append(["maim", path])
    for cmd in commands:
        try:
            subprocess.run(cmd, check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if os.path.getsize(path) > 0:
                return path
        except (OSError, subprocess.SubprocessError):
            continue
    try:
        os.unlink(path)
    except OSError:
        pass
    return None


def grab_screen(multi_monitor: bool = True) -> Snapshot | None:
    """현재 화면을 정지 이미지로 획득한다."""
    geometry = virtual_geometry(multi_monitor)
    if not is_wayland():
        pm = _grab_qt(geometry)
        if pm is not None:
            return Snapshot(pm, geometry)
    path = _portal_screenshot(False) or _external_screenshot()
    if path:
        pm = QPixmap(path)
        try:
            os.unlink(path)
        except OSError:
            pass
        if not pm.isNull():
            pm.setDevicePixelRatio(1.0)
            full = virtual_geometry(True)
            snap = Snapshot(pm, full)
            if not multi_monitor and full != geometry:
                return Snapshot(snap.crop(geometry), geometry)
            return snap
    pm = _grab_qt(geometry)
    if pm is not None:
        return Snapshot(pm, geometry)
    return None


def grab_live(rect: QRect) -> QPixmap | None:
    """돋보기 등 실시간 미리보기용 빠른 캡처 (X11 / 비 리눅스 전용)."""
    if is_wayland():
        return None
    screen = QGuiApplication.screenAt(rect.center()) or QGuiApplication.primaryScreen()
    if screen is None:
        return None
    if sys.platform == "linux":
        pm = screen.grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height())
    else:
        g = screen.geometry()
        pm = screen.grabWindow(0, rect.x() - g.x(), rect.y() - g.y(), rect.width(), rect.height())
    if pm.isNull():
        return None
    return pm


# --- X11 윈도우 정보 ---------------------------------------------------------


def _run(cmd: list[str]) -> str:
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=3).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def active_window_rect() -> tuple[QRect, int] | None:
    """활성 윈도우의 (영역, pid). X11 + xdotool 필요."""
    if not is_x11() or not has_tool("xdotool"):
        return None
    out = _run(["xdotool", "getactivewindow", "getwindowgeometry", "--shell"])
    vals = {}
    for line in out.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            vals[k.strip()] = v.strip()
    try:
        rect = QRect(int(vals["X"]), int(vals["Y"]), int(vals["WIDTH"]), int(vals["HEIGHT"]))
    except (KeyError, ValueError):
        return None
    pid_out = _run(["xdotool", "getactivewindow", "getwindowpid"]).strip()
    pid = int(pid_out) if pid_out.isdigit() else -1
    return rect, pid


def window_rects() -> list[tuple[QRect, str]]:
    """화면에 보이는 최상위 윈도우 목록 (위쪽 윈도우가 먼저). X11 전용."""
    if not is_x11() or not has_tool("xprop") or not has_tool("xwininfo"):
        return []
    out = _run(["xprop", "-root", "_NET_CLIENT_LIST_STACKING"])
    if "#" not in out:
        return []
    ids = [w.strip() for w in out.split("#", 1)[1].split(",") if w.strip()]
    result: list[tuple[QRect, str]] = []
    own_pid = os.getpid()
    for wid in reversed(ids):
        info = _run(["xwininfo", "-id", wid])
        vals = {}
        title = ""
        for line in info.splitlines():
            line = line.strip()
            if line.startswith("xwininfo: Window id:"):
                if '"' in line:
                    title = line.split('"', 1)[1].rsplit('"', 1)[0]
            elif ":" in line:
                k, v = line.split(":", 1)
                vals[k.strip()] = v.strip()
        if vals.get("Map State") != "IsViewable":
            continue
        try:
            x = int(vals["Absolute upper-left X"])
            y = int(vals["Absolute upper-left Y"])
            w = int(vals["Width"])
            h = int(vals["Height"])
        except (KeyError, ValueError):
            continue
        pid_info = _run(["xprop", "-id", wid, "_NET_WM_PID"])
        if pid_info.strip().endswith(str(own_pid)):
            continue
        if w > 1 and h > 1:
            result.append((QRect(x, y, w, h), title))
    return result


def cursor_pos() -> QPoint:
    return QCursor.pos()
