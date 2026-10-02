#include "capture/FrozenOverlay.h"

#include "core/System.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace mm::capture {

FrozenOverlay::FrozenOverlay(const Snapshot &snapshot, bool magnifier, int zoom)
    : QWidget(nullptr)
    , m_showMagnifier(magnifier)
    , m_zoom(qBound(2, zoom, 20))
    , m_snapshot(snapshot)
    , m_image(snapshot.pixmap.toImage())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
}

QPointF FrozenOverlay::toGlobal(const QPointF &p) const
{
    const QRect g = m_snapshot.geometry;
    const double sx = g.width() / double(qMax(1, width()));
    const double sy = g.height() / double(qMax(1, height()));
    return {g.x() + p.x() * sx, g.y() + p.y() * sy};
}

QRect FrozenOverlay::rectToGlobal(const QRect &r) const
{
    const QPointF tl = toGlobal(QPointF(r.topLeft()));
    const QPointF br = toGlobal(QPointF(r.x() + r.width(), r.y() + r.height()));
    return QRect(QPoint(qRound(tl.x()), qRound(tl.y())), QPoint(qRound(br.x()) - 1, qRound(br.y()) - 1));
}

QRect FrozenOverlay::fromGlobalRect(const QRect &r) const
{
    const QRect g = m_snapshot.geometry;
    const double sx = width() / double(qMax(1, g.width()));
    const double sy = height() / double(qMax(1, g.height()));
    return QRect(qRound((r.x() - g.x()) * sx), qRound((r.y() - g.y()) * sy), qRound(r.width() * sx),
                 qRound(r.height() * sy));
}

QPoint FrozenOverlay::imagePos(const QPoint &p) const
{
    return {int(p.x() * double(m_image.width()) / qMax(1, width())),
            int(p.y() * double(m_image.height()) / qMax(1, height()))};
}

QColor FrozenOverlay::colorAt(const QPoint &p) const
{
    const QPoint ip = imagePos(p);
    if (m_image.rect().contains(ip))
        return m_image.pixelColor(ip);
    return QColor(0, 0, 0);
}

void FrozenOverlay::start()
{
    if (sys::isWayland()) {
        showFullScreen();
    } else {
        setGeometry(m_snapshot.geometry);
        show();
    }
    raise();
    activateWindow();
    setFocus();
    QTimer::singleShot(50, this, &FrozenOverlay::grabInput);
}

void FrozenOverlay::grabInput()
{
    activateWindow();
    setFocus();
}

void FrozenOverlay::cancel()
{
    Q_EMIT cancelled();
    close();
}

void FrozenOverlay::paintBackground(QPainter &painter)
{
    painter.drawPixmap(rect(), m_snapshot.pixmap);
}

void FrozenOverlay::drawMagnifier(QPainter &painter, const QStringList &extraLines)
{
    if (!m_showMagnifier || !rect().contains(m_mouse))
        return;
    const int cells = 15;
    const int box = cells * m_zoom;
    const QPoint ip = imagePos(m_mouse);
    const QRect src(ip.x() - cells / 2, ip.y() - cells / 2, cells, cells);
    const QColor color = colorAt(m_mouse);
    const QPointF gp = toGlobal(QPointF(m_mouse));
    QStringList lines{
        QStringLiteral("%1, %2").arg(int(gp.x())).arg(int(gp.y())),
        QStringLiteral("RGB(%1, %2, %3)  %4")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.name().toUpper()),
    };
    lines << extraLines;

    const int textH = 18 * int(lines.size()) + 6;
    int x = m_mouse.x() + 24;
    int y = m_mouse.y() + 24;
    if (x + box > width())
        x = m_mouse.x() - 24 - box;
    if (y + box + textH > height())
        y = m_mouse.y() - 24 - box - textH;
    const QRect target(x, y, box, box);

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.fillRect(target.adjusted(-2, -2, 2, textH + 2), QColor(30, 30, 30, 230));
    painter.drawImage(QRectF(target), m_image, QRectF(src));
    painter.setPen(QPen(QColor(47, 140, 255, 200), 1));
    const QPoint mid = target.topLeft() + QPoint((cells / 2) * m_zoom, (cells / 2) * m_zoom);
    const int half = m_zoom / 2;
    painter.drawRect(QRect(mid, QSize(m_zoom, m_zoom)));
    painter.drawLine(target.left(), mid.y() + half, mid.x(), mid.y() + half);
    painter.drawLine(mid.x() + m_zoom, mid.y() + half, target.right(), mid.y() + half);
    painter.drawLine(mid.x() + half, target.top(), mid.x() + half, mid.y());
    painter.drawLine(mid.x() + half, mid.y() + m_zoom, mid.x() + half, target.bottom());
    painter.setPen(Qt::white);
    QFont f = painter.font();
    f.setPixelSize(12);
    painter.setFont(f);
    for (int i = 0; i < lines.size(); ++i)
        painter.drawText(QRect(x + 4, y + box + 4 + i * 18, box + 120, 18), Qt::AlignLeft, lines.at(i));
    painter.restore();
}

void FrozenOverlay::drawHint(QPainter &painter, const QString &text)
{
    painter.save();
    QFont f = painter.font();
    f.setPixelSize(13);
    painter.setFont(f);
    const int w = painter.fontMetrics().horizontalAdvance(text) + 28;
    int topCenter = width() / 2;
    QScreen *screen = QGuiApplication::screenAt(mapToGlobal(QPoint(width() / 2, 10)));
    if (screen && !sys::isWayland()) {
        const QRect sg = screen->geometry().translated(-m_snapshot.geometry.topLeft());
        topCenter = sg.center().x();
    }
    QRect r(topCenter - w / 2, 12, w, 30);
    if (r.contains(m_mouse))
        r.moveTop(height() - 50);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 20, 210));
    painter.drawRoundedRect(r, 6, 6);
    painter.setPen(Qt::white);
    painter.drawText(r, Qt::AlignCenter, text);
    painter.restore();
}

void FrozenOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    QPoint move;
    switch (event->key()) {
    case Qt::Key_Left: move = QPoint(-step, 0); break;
    case Qt::Key_Right: move = QPoint(step, 0); break;
    case Qt::Key_Up: move = QPoint(0, -step); break;
    case Qt::Key_Down: move = QPoint(0, step); break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    QCursor::setPos(mapToGlobal(m_mouse + move));
}

} // namespace mm::capture
