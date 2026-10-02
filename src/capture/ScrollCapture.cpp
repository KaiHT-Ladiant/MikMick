#include "capture/ScrollCapture.h"

#include "capture/Backend.h"
#include "capture/X11.h"
#include "core/System.h"

#include <QCursor>
#include <QProcess>

#include <cstring>

namespace mm::capture {

namespace {

QImage normalized(const QImage &img)
{
    return img.format() == QImage::Format_ARGB32 ? img : img.convertToFormat(QImage::Format_ARGB32);
}

bool rowEqual(const QImage &a, int ra, const QImage &b, int rb, size_t bytes)
{
    return std::memcmp(a.constScanLine(ra), b.constScanLine(rb), bytes) == 0;
}

QList<quint64> rowHashes(const QImage &img, size_t bytes)
{
    QList<quint64> out;
    out.reserve(img.height());
    for (int y = 0; y < img.height(); ++y) {
        const auto *p = img.constScanLine(y);
        quint64 h = 1469598103934665603ULL;
        for (size_t i = 0; i < bytes; ++i) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        out << h;
    }
    return out;
}

} // namespace

std::optional<int> findOverlap(const QImage &prevIn, const QImage &curIn, int minOverlap)
{
    if (prevIn.size() != curIn.size())
        return 0;
    const QImage prev = normalized(prevIn);
    const QImage cur = normalized(curIn);
    const int h = prev.height();
    const size_t bytes = size_t(prev.width()) * 4;

    bool identical = true;
    for (int y = 0; y < h && identical; ++y)
        identical = rowEqual(prev, y, cur, y, bytes);
    if (identical)
        return std::nullopt;

    const QList<quint64> prevRows = rowHashes(prev, bytes);
    const QList<quint64> curRows = rowHashes(cur, bytes);
    const int probeCount = 12;
    // Smallest scroll distance d with cur[0:h-d] == prev[d:h].
    for (int d = 1; d <= h - minOverlap; ++d) {
        const int overlap = h - d;
        const int step = qMax(1, overlap / probeCount);
        bool probes = true;
        for (int i = 0; i < overlap && probes; i += step)
            probes = prevRows[d + i] == curRows[i];
        if (!probes)
            continue;
        bool all = true;
        for (int i = 0; i < overlap && all; ++i)
            all = prevRows[d + i] == curRows[i] && rowEqual(prev, d + i, cur, i, bytes);
        if (all)
            return overlap;
    }
    return 0;
}

QImage stitch(const QList<QImage> &frames)
{
    if (frames.isEmpty())
        return {};
    struct Part
    {
        QImage image;
        int firstRow;
    };
    QList<Part> parts{{normalized(frames.first()), 0}};
    for (int i = 1; i < frames.size(); ++i) {
        const std::optional<int> k = findOverlap(frames.at(i - 1), frames.at(i));
        if (!k)
            continue;
        parts.append({normalized(frames.at(i)), *k});
    }
    int width = 0;
    int height = 0;
    for (const Part &p : std::as_const(parts)) {
        width = qMax(width, p.image.width());
        height += p.image.height() - p.firstRow;
    }
    QImage out(width, height, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    int y = 0;
    for (const Part &p : std::as_const(parts)) {
        const size_t bytes = size_t(p.image.width()) * 4;
        for (int row = p.firstRow; row < p.image.height(); ++row, ++y)
            std::memcpy(out.scanLine(y), p.image.constScanLine(row), bytes);
    }
    return out;
}

ScrollCapture::ScrollCapture(const QRect &rect, int delayMs, int maxFrames, QObject *parent)
    : QObject(parent)
    , m_rect(rect)
    , m_delayMs(qMax(30, delayMs))
    , m_maxFrames(maxFrames)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ScrollCapture::step);
}

bool ScrollCapture::supported(QString *message)
{
    QString msg;
    if (!sys::isX11() || sys::isWayland())
        msg = QStringLiteral("자동 스크롤 캡처는 현재 X11 세션에서만 지원됩니다.");
    else if (!x11::hasXTest() && !hasTool(QStringLiteral("xdotool")))
        msg = QStringLiteral("자동 스크롤 캡처에는 xdotool 이 필요합니다.\n예) sudo apt install xdotool");
    if (message)
        *message = msg;
    return msg.isEmpty();
}

void ScrollCapture::start()
{
    QString msg;
    if (!supported(&msg)) {
        Q_EMIT failed(msg);
        return;
    }
    QCursor::setPos(m_rect.center());
    m_timer.start(m_delayMs);
}

void ScrollCapture::step()
{
    const QPixmap pm = grabLive(m_rect);
    if (pm.isNull()) {
        Q_EMIT failed(QStringLiteral("화면을 캡처할 수 없습니다."));
        return;
    }
    const QImage frame = normalized(pm.toImage());
    if (!m_frames.isEmpty() && !findOverlap(m_frames.last(), frame)) {
        done();
        return;
    }
    m_frames << frame;
    if (m_frames.size() >= m_maxFrames) {
        done();
        return;
    }
    scroll();
    m_timer.start(m_delayMs);
}

void ScrollCapture::scroll()
{
    if (x11::clickButton(5, 3, 10))
        return;
    QProcess::execute(QStringLiteral("xdotool"), {QStringLiteral("click"), QStringLiteral("--repeat"),
                                                   QStringLiteral("3"), QStringLiteral("--delay"),
                                                   QStringLiteral("10"), QStringLiteral("5")});
}

void ScrollCapture::done()
{
    if (m_frames.isEmpty()) {
        Q_EMIT failed(QStringLiteral("캡처된 이미지가 없습니다."));
        return;
    }
    Q_EMIT finished(stitch(m_frames));
}

} // namespace mm::capture
