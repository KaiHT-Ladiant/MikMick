#pragma once

#include "capture/Backend.h"
#include "capture/FrozenOverlay.h"
#include "core/Config.h"

#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <optional>

class QTimer;

namespace mm::tools {

// Live zoom of the area around the cursor. Uses the snapshot when given (Wayland), else grabLive().
class Magnifier : public QWidget
{
    Q_OBJECT
public:
    Magnifier(Config &config, std::optional<capture::Snapshot> snapshot, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void tick();

    std::optional<capture::Snapshot> m_snapshot;
    int m_zoom = 6;
    bool m_follow = true;
    QPixmap m_frame;
    QTimer *m_timer = nullptr;
};

// Translucent on-screen ruler (units px / in / cm, persisted in config ruler.*).
class Ruler : public QWidget
{
    Q_OBJECT
public:
    explicit Ruler(Config &config, QWidget *parent = nullptr);

    void start();
    void toggleOrientation();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    double pixelsPerUnit() const;
    bool nearEnd(const QPoint &pos) const;
    void setUnit(const QString &unit);
    void askDpi();

    Config &m_config;
    bool m_horizontal = true;
    QString m_unit;
    int m_dpi = 96;
    std::optional<QPoint> m_drag;
    bool m_resize = false;
    QTimer *m_timer = nullptr;
};

// Crosshair with absolute coordinates and coordinates relative to a clicked origin.
class CrosshairOverlay : public capture::FrozenOverlay
{
    Q_OBJECT
public:
    CrosshairOverlay(const capture::Snapshot &snap, int zoom = 6);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    std::optional<QPoint> m_origin;
    QList<QPoint> m_marks;
};

// Protractor: click the vertex, then a point on each of the two legs.
class ProtractorOverlay : public capture::FrozenOverlay
{
    Q_OBJECT
public:
    ProtractorOverlay(const capture::Snapshot &snap, int zoom = 6);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void drawLabel(QPainter &p, const QPointF &v, const QString &text);

    QList<QPointF> m_points;
};

// Live grabbing is impossible on Wayland, so magnifier-like tools need a frozen snapshot there.
bool needsSnapshotForLiveTools();

} // namespace mm::tools
