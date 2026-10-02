#!/bin/sh
# Installs a built MikMick package on the running distro (container) and checks that it
# starts on an X server with all shared libraries resolved.
#   install-test.sh deb|rpm|arch|appimage <dir containing the package>
set -eu

kind=$1
case $kind in
    deb) pattern='*.deb' ;;
    rpm) pattern='*.rpm' ;;
    arch) pattern='*.pkg.tar.zst' ;;
    appimage) pattern='*.AppImage' ;;
    *) echo "unknown package kind: $kind" >&2; exit 2 ;;
esac
pkg=
for f in "$2"/$pattern; do
    case $f in *-debug-*) continue ;; esac
    [ -f "$f" ] && pkg=$(cd "$(dirname "$f")" && pwd)/$(basename "$f")
done
[ -n "$pkg" ] || { echo "no $kind package in $2" >&2; exit 1; }
echo "== $kind: $pkg"
. /etc/os-release && echo "== on $PRETTY_NAME"

if command -v apt-get >/dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq xvfb >/dev/null
    case $kind in
        deb) apt-get install -y "$pkg" ;;
        appimage) apt-get install -y -qq libgl1 libegl1 libfontconfig1 libharfbuzz0b libcom-err2 libgpg-error0 libsm6 libice6 libx11-6 libx11-xcb1 libxcb1 >/dev/null ;;
    esac
elif command -v dnf >/dev/null; then
    dnf install -y -q xorg-x11-server-Xvfb
    case $kind in
        rpm) dnf install -y --nogpgcheck "$pkg" ;;
        appimage) dnf install -y -q libglvnd-glx libglvnd-egl fontconfig harfbuzz libcom_err libgpg-error libSM libICE libX11 libX11-xcb libxcb ;;
    esac
elif command -v zypper >/dev/null; then
    zypper -n -q install xorg-x11-server-Xvfb
    case $kind in
        rpm) zypper -n --no-gpg-checks install --allow-unsigned-rpm "$pkg" ;;
        appimage) zypper -n -q install Mesa-libGL1 Mesa-libEGL1 fontconfig libharfbuzz0 libcom_err2 libgpg-error0 libSM6 libICE6 libX11-6 libX11-xcb1 libxcb1 ;;
    esac
elif command -v pacman >/dev/null; then
    pacman -Syu --noconfirm --needed xorg-server-xvfb
    case $kind in
        arch) pacman -U --noconfirm "$pkg" ;;
        appimage) pacman -S --noconfirm --needed libglvnd fontconfig harfbuzz e2fsprogs libgpg-error libsm libice libx11 libxcb ;;
    esac
else
    echo "unsupported distro" >&2
    exit 1
fi

Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -S /tmp/.X11-unix/X99 ] && break
    sleep 1
done
[ -S /tmp/.X11-unix/X99 ] || { cat /tmp/xvfb.log; echo "Xvfb did not start" >&2; exit 1; }
export DISPLAY=:99 QT_QPA_PLATFORM=xcb

if [ "$kind" = appimage ]; then
    chmod +x "$pkg"
    (cd /tmp && "$pkg" --appimage-extract >/dev/null)
    missing=$(find /tmp/squashfs-root -type f \( -name '*.so*' -o -perm -u+x \) -exec ldd {} \; 2>/dev/null |
        grep 'not found' | sort -u || true)
    if [ -n "$missing" ]; then
        echo "== libraries missing on this system:"
        echo "$missing"
        exit 1
    fi
    set -- "$pkg" --appimage-extract-and-run --version
else
    bin=$(command -v mikmick)
    if ldd "$bin" | grep 'not found'; then
        exit 1
    fi
    test -f /usr/share/applications/io.github.kaiht_ladiant.MikMick.desktop
    test -f /usr/share/icons/hicolor/scalable/apps/mikmick.svg
    set -- mikmick --version
fi

if ! out=$(timeout 60 "$@"); then
    echo "== failed or timed out, rerunning with Qt debug output" >&2
    QT_DEBUG_PLUGINS=1 QT_LOGGING_RULES='qt.*.debug=true' timeout 30 "$@" 2>&1 | tail -n 80
    exit 1
fi
echo "$out"
echo "$out" | grep -q '^MikMick '
echo "== OK"
