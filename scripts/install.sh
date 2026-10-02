#!/usr/bin/env bash
# 믹믹(MikMick) 빌드 & 사용자 단위 설치 스크립트
#   ./scripts/install.sh               빌드 후 ~/.local 에 설치
#   PREFIX=/usr/local sudo -E ./scripts/install.sh   시스템 전체 설치
#   ./scripts/install.sh --uninstall   제거
set -euo pipefail

APP_ID="io.github.kaiht_ladiant.MikMick"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

uninstall() {
    rm -f "$PREFIX/bin/mikmick"
    rm -f "$PREFIX/share/applications/$APP_ID.desktop"
    rm -f "$PREFIX/share/metainfo/$APP_ID.metainfo.xml"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/mikmick.svg"
    rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/mikmick.desktop"
    echo "믹믹을 제거했습니다. (설정 파일 ~/.config/mikmick 은 남겨 두었습니다)"
}

if [[ "${1:-}" == "--uninstall" ]]; then
    uninstall
    exit 0
fi

missing=()
command -v cmake >/dev/null 2>&1 || missing+=("cmake")
command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 || missing+=("C++ 컴파일러(g++)")
if ! command -v qmake6 >/dev/null 2>&1 && ! ls /usr/lib*/cmake/Qt6/Qt6Config.cmake /usr/lib/*/cmake/Qt6/Qt6Config.cmake >/dev/null 2>&1; then
    missing+=("Qt6 개발 패키지")
fi
if (( ${#missing[@]} )); then
    echo "다음 패키지가 필요합니다: ${missing[*]}"
    echo "  Debian/Ubuntu/Kali : sudo apt install cmake ninja-build g++ qt6-base-dev libxcb1-dev libxcb-keysyms1-dev libxcb-xtest0-dev libxcb-xfixes0-dev"
    echo "  Fedora             : sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel libxcb-devel xcb-util-keysyms-devel"
    echo "  Arch               : sudo pacman -S cmake ninja gcc qt6-base libxcb xcb-util-keysyms"
    exit 1
fi

generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

echo "==> 빌드합니다 ($BUILD_DIR)"
cmake -S "$ROOT" -B "$BUILD_DIR" "${generator[@]}" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DMIKMICK_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel

echo "==> $PREFIX 에 설치합니다"
cmake --install "$BUILD_DIR"

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$PREFIX/share/applications" || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q "$PREFIX/share/icons/hicolor" || true

echo
echo "설치 완료! 앱 메뉴에서 '믹믹'을 실행하거나 터미널에서 'mikmick' 을 입력하세요."
case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "주의: $PREFIX/bin 이 PATH 에 없습니다. ~/.bashrc 등에 export PATH=\"$PREFIX/bin:\$PATH\" 를 추가하세요." ;;
esac
