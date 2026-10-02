#!/usr/bin/env bash
# 믹믹(MikMick) 사용자 단위 설치 스크립트
#   ./scripts/install.sh            설치
#   ./scripts/install.sh --uninstall 제거
set -euo pipefail

APP_ID="io.github.kaiht_ladiant.MikMick"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}"
VENV="$DATA_DIR/mikmick/venv"
BIN="$PREFIX/bin"

uninstall() {
    if command -v pipx >/dev/null 2>&1 && pipx list 2>/dev/null | grep -q "package mikmick"; then
        pipx uninstall mikmick || true
    fi
    rm -rf "$DATA_DIR/mikmick"
    rm -f "$BIN/mikmick"
    rm -f "$DATA_DIR/applications/$APP_ID.desktop"
    rm -f "$DATA_DIR/icons/hicolor/scalable/apps/mikmick.svg"
    rm -f "$DATA_DIR/metainfo/$APP_ID.metainfo.xml"
    rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/mikmick.desktop"
    echo "믹믹을 제거했습니다. (설정 파일: ~/.config/mikmick 은 남겨 두었습니다)"
}

if [[ "${1:-}" == "--uninstall" ]]; then
    uninstall
    exit 0
fi

if command -v pipx >/dev/null 2>&1; then
    echo "==> pipx 로 설치합니다"
    pipx install --force "$ROOT[full]"
else
    echo "==> 가상환경($VENV)에 설치합니다"
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --upgrade pip >/dev/null
    "$VENV/bin/pip" install "$ROOT[full]"
    mkdir -p "$BIN"
    ln -sf "$VENV/bin/mikmick" "$BIN/mikmick"
fi

install -Dm644 "$ROOT/packaging/$APP_ID.desktop" "$DATA_DIR/applications/$APP_ID.desktop"
install -Dm644 "$ROOT/packaging/mikmick.svg" "$DATA_DIR/icons/hicolor/scalable/apps/mikmick.svg"
install -Dm644 "$ROOT/packaging/$APP_ID.metainfo.xml" "$DATA_DIR/metainfo/$APP_ID.metainfo.xml"
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$DATA_DIR/applications" || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q "$DATA_DIR/icons/hicolor" || true

echo
echo "설치 완료! 앱 메뉴에서 '믹믹'을 실행하거나 터미널에서 'mikmick' 을 입력하세요."
case ":$PATH:" in
    *":$BIN:"*) ;;
    *) echo "주의: $BIN 이 PATH 에 없습니다. ~/.bashrc 등에 export PATH=\"$BIN:\$PATH\" 를 추가하세요." ;;
esac
