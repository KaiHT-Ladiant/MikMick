#pragma once

#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>

#include <optional>

namespace mm::capture {

// A still image of the screen and the logical (global) area it covers.
struct Snapshot
{
    QPixmap pixmap;
    QRect geometry;

    bool isNull() const { return pixmap.isNull(); }
    double scaleX() const { return pixmap.width() / double(qMax(1, geometry.width())); }
    double scaleY() const { return pixmap.height() / double(qMax(1, geometry.height())); }
    // Cuts out a rect given in global logical coordinates (device pixel ratio reset to 1).
    QPixmap crop(const QRect &rect) const;
};

struct WindowInfo
{
    QRect rect;      // global logical coordinates, including the WM frame when known
    QString title;
    qint64 pid = -1;
    quint32 id = 0;  // X11 window id (0 when unknown)
};

bool hasTool(const QString &name);

// Union of all screens, or the screen under the cursor when multiMonitor is false.
QRect virtualGeometry(bool multiMonitor = true);

// Freezes the current screen.
// X11: QScreen::grabWindow. Wayland: xdg-desktop-portal Screenshot (QtDBus),
// then grim / gnome-screenshot / spectacle / scrot / maim.
std::optional<Snapshot> grabScreen(bool multiMonitor = true);

// Fast live grab for magnifier-like previews. Returns a null pixmap on Wayland.
QPixmap grabLive(const QRect &rect);

// X11 only (empty on Wayland / without XCB support).
std::optional<WindowInfo> activeWindow();
// Visible top-level windows, topmost first, excluding this process.
QList<WindowInfo> windowList();

} // namespace mm::capture
