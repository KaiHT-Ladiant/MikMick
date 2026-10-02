#pragma once

#include "capture/Backend.h"
#include "capture/FrozenOverlay.h"

#include <QList>
#include <QPainterPath>
#include <QRect>
#include <QSize>
#include <QString>

#include <optional>

namespace mm::capture {

// Area selector for the region / fixed / freehand / window modes.
class CaptureOverlay : public FrozenOverlay
{
    Q_OBJECT
public:
    explicit CaptureOverlay(const Snapshot &snapshot, const QString &mode = QStringLiteral("region"),
                            bool magnifier = true, int zoom = 6, const QSize &fixedSize = QSize(640, 480),
                            const QList<WindowInfo> &windows = {}, bool showHint = true);

    QString mode() const { return m_mode; }
    QSize fixedSize() const { return m_fixedSize; }

    static QString hint(const QString &mode);

Q_SIGNALS:
    // globalRect: global logical coordinates. freehandPath: in pixels of Snapshot::crop(globalRect),
    // empty unless the mode is freehand.
    void selected(const QRect &globalRect, const QPainterPath &freehandPath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRect fixedRect(const QPoint &center) const;
    std::optional<QRect> windowAt(const QPoint &p) const;
    void finish(const QRect &rect, const QPainterPath &path = QPainterPath());

    QString m_mode;
    QSize m_fixedSize;
    QList<WindowInfo> m_windows;
    bool m_showHint = true;
    std::optional<QPoint> m_origin;
    QRect m_current;
    QPainterPath m_path;
    bool m_dragging = false;
    std::optional<QRect> m_hoverWindow;
};

} // namespace mm::capture
