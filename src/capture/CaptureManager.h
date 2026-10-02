#pragma once

#include "capture/Backend.h"

#include <QDialog>
#include <QImage>
#include <QObject>
#include <QPainterPath>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>

#include <functional>

class QSpinBox;

namespace mm {
class Config;
}

namespace mm::capture {

class CaptureOverlay;
class ScrollCapture;

// Default arrow cursor drawn at pos (image pixels).
void drawCursor(QImage &image, const QPoint &pos);
// canberra-gtk-play / paplay shutter sound, QApplication::beep() as fallback.
void playShutter();

class FixedSizeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FixedSizeDialog(const QSize &size, QWidget *parent = nullptr);
    QSize value() const;

private:
    QSpinBox *m_width;
    QSpinBox *m_height;
};

class CaptureManager : public QObject
{
    Q_OBJECT
public:
    // hideWindows returns true when it hid something (wait for the compositor before grabbing).
    CaptureManager(Config &config, std::function<bool()> hideWindows, std::function<void()> restoreWindows,
                   QObject *parent = nullptr);

    void capture(const QString &mode, int delayMs = -1); // delayMs < 0: use config capture.delay_ms
    bool busy() const { return m_busy; }

Q_SIGNALS:
    void captured(const QImage &image, const QString &mode);
    void failed(const QString &message);

private:
    std::optional<Snapshot> snapshot();
    void run(QString mode);
    void onSelected(const Snapshot &snap, const QRect &rect, const QPainterPath &path, const QString &mode);
    void deliver(QImage image, const QRect &rect, const QString &mode, bool remember = true);
    void onCancelled();
    void fail(const QString &message);

    Config &m_config;
    std::function<bool()> m_hideWindows;
    std::function<void()> m_restoreWindows;
    bool m_busy = false;
    bool m_awaitingOverlay = false;
    QPoint m_cursor;
    QPointer<CaptureOverlay> m_overlay;
    QPointer<ScrollCapture> m_scroll;
};

} // namespace mm::capture
