"""캡처 결과 출력: 파일 저장, 자동 저장, 클립보드, FTP, 외부 프로그램."""

from __future__ import annotations

import ftplib
import os
import shlex
import subprocess
import webbrowser
from pathlib import Path

from PySide6.QtCore import QMarginsF, QRect, QSizeF, Qt
from PySide6.QtGui import QGuiApplication, QImage, QPageSize, QPainter, QPdfWriter

from mikmick.config import Config, pictures_dir
from mikmick.utils.filename import expand_pattern, ext_of, unique_path


def save_image(image: QImage, path: str, config: Config) -> bool:
    ext = ext_of(path) or "png"
    if ext == "pdf":
        writer = QPdfWriter(path)
        writer.setResolution(96)
        writer.setPageSize(QPageSize(QSizeF(image.width(), image.height()), QPageSize.Unit.Point))
        writer.setPageMargins(QMarginsF(0, 0, 0, 0))
        painter = QPainter(writer)
        target = painter.viewport()
        scaled = image.size().scaled(target.size(), Qt.AspectRatioMode.KeepAspectRatio)
        painter.drawImage(QRect(target.topLeft(), scaled), image)
        painter.end()
        return os.path.exists(path)
    fmt = {"jpg": "JPEG", "jpeg": "JPEG", "png": "PNG", "bmp": "BMP", "gif": "GIF", "webp": "WEBP", "tiff": "TIFF", "tif": "TIFF"}.get(
        ext, "PNG"
    )
    quality = int(config.get("image", "jpeg_quality", 100)) if fmt in ("JPEG", "WEBP") else -1
    if fmt in ("JPEG", "BMP"):
        image = image.convertToFormat(QImage.Format.Format_RGB32)
    return image.save(path, fmt, quality)


def next_filename(config: Config) -> tuple[str, str]:
    """설정의 파일 이름 패턴으로 (이름, 확장자) 를 생성하고 카운터를 증가시킨다."""
    counter = int(config.get("filename", "counter", 0))
    name = expand_pattern(config.get("filename", "pattern", "%c"), counter)
    config.set("filename", "counter", counter + 1)
    config.save()
    return name, config.get("filename", "format", "png")


def autosave_folder(config: Config) -> Path:
    folder = config.get("autosave", "folder", "")
    path = Path(folder) if folder else pictures_dir() / "MikMick"
    path.mkdir(parents=True, exist_ok=True)
    return path


def autosave(image: QImage, config: Config) -> str | None:
    name, ext = next_filename(config)
    path = unique_path(autosave_folder(config), name, ext)
    if save_image(image, str(path), config):
        return str(path)
    return None


def temp_save(image: QImage, config: Config) -> str | None:
    import tempfile

    name, ext = next_filename(config)
    folder = Path(tempfile.gettempdir()) / "mikmick"
    folder.mkdir(parents=True, exist_ok=True)
    path = unique_path(folder, name, ext)
    if save_image(image, str(path), config):
        return str(path)
    return None


def copy_to_clipboard(image: QImage) -> None:
    QGuiApplication.clipboard().setImage(image)


def run_program(path: str, config: Config) -> bool:
    program = config.get("autosave", "program", "").strip()
    if not program:
        return False
    args = config.get("autosave", "program_args", "").strip()
    if "%f" in args:
        arg_list = [a.replace("%f", path) for a in shlex.split(args)]
    else:
        arg_list = shlex.split(args) + [path]
    try:
        subprocess.Popen([program, *arg_list], start_new_session=True)
        return True
    except OSError:
        return False


def open_with_default(path: str) -> None:
    try:
        subprocess.Popen(["xdg-open", path], start_new_session=True)
    except OSError:
        webbrowser.open(Path(path).as_uri())


def send_email(path: str) -> bool:
    try:
        subprocess.Popen(["xdg-email", "--attach", path], start_new_session=True)
        return True
    except OSError:
        return False


def ftp_connect(config: Config) -> ftplib.FTP:
    ftp = ftplib.FTP()
    ftp.connect(config.get("ftp", "server", ""), int(config.get("ftp", "port", 21)), timeout=15)
    ftp.login(config.get("ftp", "user", "anonymous") or "anonymous", config.get("ftp", "password", ""))
    ftp.set_pasv(bool(config.get("ftp", "passive", False)))
    remote = config.get("ftp", "path", "").strip()
    if remote:
        ftp.cwd(remote)
    return ftp


def ftp_test(config: Config) -> str:
    ftp = ftp_connect(config)
    try:
        return ftp.getwelcome() or "OK"
    finally:
        try:
            ftp.quit()
        except ftplib.all_errors:
            ftp.close()


def ftp_upload(path: str, config: Config) -> str:
    """파일을 업로드하고 공개 URL (설정된 경우) 을 반환."""
    ftp = ftp_connect(config)
    name = os.path.basename(path)
    try:
        with open(path, "rb") as fp:
            ftp.storbinary(f"STOR {name}", fp)
    finally:
        try:
            ftp.quit()
        except ftplib.all_errors:
            ftp.close()
    base = config.get("ftp", "url", "").strip()
    url = (base.rstrip("/") + "/" + name) if base else ""
    if url:
        if config.get("ftp", "copy_url", False):
            QGuiApplication.clipboard().setText(url)
        if config.get("ftp", "open_url", False):
            webbrowser.open(url)
    return url
