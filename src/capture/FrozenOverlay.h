#pragma once

#include "capture/Backend.h"

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

namespace mm::capture {

// Fullscreen window showing a frozen screenshot. Base for selection overlays and pickers.
class FrozenOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit FrozenOverlay(const Snapshot &snapshot, bool magnifier = true, int zoom = 6);

    const Snapshot &snapshot() const { return m_snapshot; }
    const QImage &image() const { return m_image; }

    // Widget coordinates <-> global logical coordinates / snapshot pixels.
    QPointF toGlobal(const QPointF &p) const;
    QRect rectToGlobal(const QRect &r) const;
    QRect fromGlobalRect(const QRect &r) const;
    QPoint imagePos(const QPoint &p) const;
    QColor colorAt(const QPoint &p) const;

    // Shows the overlay over the snapshot area (fullscreen on Wayland) and grabs focus.
    void start();
    void cancel();

Q_SIGNALS:
    void cancelled();

protected:
    void paintBackground(QPainter &painter);
    // Zoomed pixel grid next to the cursor, with coordinates, colour and extra lines.
    void drawMagnifier(QPainter &painter, const QStringList &extraLines = {});
    // Rounded hint bubble at the top centre of the current screen.
    void drawHint(QPainter &painter, const QString &text);
    void keyPressEvent(QKeyEvent *event) override;

    QPoint m_mouse{-1000, -1000};
    bool m_showMagnifier = true;
    int m_zoom = 6;

private:
    void grabInput();

    Snapshot m_snapshot;
    QImage m_image;
};

} // namespace mm::capture
