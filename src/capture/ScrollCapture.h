#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QRect>
#include <QTimer>

#include <optional>

namespace mm::capture {

// Height of the rows shared by the bottom of prev and the top of cur (cur rows from that index on
// are new). std::nullopt: frames identical. 0: different sizes or no overlap found.
std::optional<int> findOverlap(const QImage &prev, const QImage &cur, int minOverlap = 8);
// Appends the new part of each frame below the previous ones (ARGB32).
QImage stitch(const QList<QImage> &frames);

// Scrolls the area under rect with the mouse wheel (XTest, xdotool as fallback) and stitches the frames.
class ScrollCapture : public QObject
{
    Q_OBJECT
public:
    explicit ScrollCapture(const QRect &rect, int delayMs = 100, int maxFrames = 60, QObject *parent = nullptr);

    // X11 only. On failure message receives the Korean explanation.
    static bool supported(QString *message = nullptr);
    void start();

Q_SIGNALS:
    void finished(const QImage &image);
    void failed(const QString &message);

private:
    void step();
    void scroll();
    void done();

    QRect m_rect;
    int m_delayMs;
    int m_maxFrames;
    QList<QImage> m_frames;
    QTimer m_timer;
};

} // namespace mm::capture
