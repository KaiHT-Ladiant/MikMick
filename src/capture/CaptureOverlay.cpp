#include "capture/CaptureOverlay.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTransform>
#include <QWheelEvent>

namespace mm::capture {

namespace {

const QColor kDim(0, 0, 0, 110);
const QColor kAccent(0x2f, 0x8c, 0xff);

} // namespace

CaptureOverlay::CaptureOverlay(const Snapshot &snapshot, const QString &mode, bool magnifier, int zoom,
                               const QSize &fixedSize, const QList<WindowInfo> &windows, bool showHint)
    : FrozenOverlay(snapshot, magnifier, zoom)
    , m_mode(mode)
    , m_fixedSize(fixedSize.isValid() && !fixedSize.isEmpty() ? fixedSize : QSize(640, 480))
    , m_windows(windows)
    , m_showHint(showHint)
{
}

QString CaptureOverlay::hint(const QString &mode)
{
    if (mode == QLatin1String("region"))
        return QStringLiteral("드래그하여 캡처할 영역을 지정하세요.  ESC: 취소   방향키: 1px 이동");
    if (mode == QLatin1String("fixed"))
        return QStringLiteral("클릭하여 고정된 영역을 캡처하세요.  휠: 크기 변경   ESC: 취소");
    if (mode == QLatin1String("freehand"))
        return QStringLiteral("마우스로 자유롭게 영역을 그리세요.  ESC: 취소");
    if (mode == QLatin1String("window"))
        return QStringLiteral("캡처할 윈도우를 클릭하세요. 드래그하면 영역을 지정합니다.  ESC: 취소");
    return {};
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (m_dragging) {
            m_dragging = false;
            m_origin.reset();
            m_current = QRect();
            m_path = QPainterPath();
            update();
        } else {
            cancel();
        }
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    const QPoint pos = event->position().toPoint();
    if (m_mode == QLatin1String("fixed")) {
        finish(fixedRect(pos));
        return;
    }
    m_origin = pos;
    m_dragging = true;
    if (m_mode == QLatin1String("freehand"))
        m_path = QPainterPath(QPointF(pos));
    m_current = QRect(pos, pos);
    update();
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_mouse = event->position().toPoint();
    if (m_dragging && m_origin) {
        if (m_mode == QLatin1String("freehand")) {
            m_path.lineTo(QPointF(m_mouse));
            m_current = m_path.boundingRect().toAlignedRect();
        } else {
            m_current = QRect(QPoint(qMin(m_origin->x(), m_mouse.x()), qMin(m_origin->y(), m_mouse.y())),
                              QSize(qAbs(m_mouse.x() - m_origin->x()), qAbs(m_mouse.y() - m_origin->y())));
        }
    } else if (m_mode == QLatin1String("window")) {
        m_hoverWindow = windowAt(m_mouse);
    }
    update();
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    if (m_mode == QLatin1String("freehand")) {
        m_path.closeSubpath();
        const QRect rect = m_path.boundingRect().toAlignedRect();
        if (rect.width() > 3 && rect.height() > 3)
            finish(rect, m_path);
        return;
    }
    const QRect rect = m_current.normalized();
    if (rect.width() < 3 || rect.height() < 3) {
        if (m_mode == QLatin1String("window") && m_hoverWindow) {
            finish(*m_hoverWindow);
            return;
        }
        m_current = QRect();
        update();
        return;
    }
    finish(rect);
}

void CaptureOverlay::wheelEvent(QWheelEvent *event)
{
    if (m_mode != QLatin1String("fixed"))
        return;
    const QPoint angle = event->angleDelta();
    const int steps = angle.y() != 0 ? angle.y() : angle.x();
    if (steps == 0)
        return;
    const int delta = steps > 0 ? 10 : -10;
    if (event->modifiers() & Qt::ShiftModifier)
        m_fixedSize.setWidth(qMax(10, m_fixedSize.width() + delta));
    else if (event->modifiers() & Qt::ControlModifier)
        m_fixedSize.setHeight(qMax(10, m_fixedSize.height() + delta));
    else
        m_fixedSize = QSize(qMax(10, m_fixedSize.width() + delta), qMax(10, m_fixedSize.height() + delta));
    update();
}

void CaptureOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_mode == QLatin1String("fixed")) {
            finish(fixedRect(m_mouse));
            return;
        }
        if (m_mode == QLatin1String("window") && m_hoverWindow) {
            finish(*m_hoverWindow);
            return;
        }
    }
    FrozenOverlay::keyPressEvent(event);
}

QRect CaptureOverlay::fixedRect(const QPoint &center) const
{
    const QRect g = snapshot().geometry;
    const double sx = width() / double(qMax(1, g.width()));
    const double sy = height() / double(qMax(1, g.height()));
    const int w = qRound(m_fixedSize.width() * sx);
    const int h = qRound(m_fixedSize.height() * sy);
    QRect r(center.x() - w / 2, center.y() - h / 2, w, h);
    if (r.left() < 0)
        r.moveLeft(0);
    if (r.top() < 0)
        r.moveTop(0);
    if (r.right() > width() - 1)
        r.moveRight(width() - 1);
    if (r.bottom() > height() - 1)
        r.moveBottom(height() - 1);
    return r;
}

std::optional<QRect> CaptureOverlay::windowAt(const QPoint &p) const
{
    const QPoint gp = toGlobal(QPointF(p)).toPoint();
    for (const WindowInfo &w : m_windows) {
        if (w.rect.contains(gp))
            return fromGlobalRect(w.rect).intersected(rect());
    }
    return std::nullopt;
}

void CaptureOverlay::finish(const QRect &selection, const QPainterPath &path)
{
    const QRect r = selection.intersected(rect());
    if (r.isEmpty())
        return;
    const QRect globalRect = rectToGlobal(r);
    QPainterPath pixmapPath;
    if (!path.isEmpty()) {
        const QRect g = snapshot().geometry;
        const double sx = snapshot().scaleX() * g.width() / qMax(1, width());
        const double sy = snapshot().scaleY() * g.height() / qMax(1, height());
        QTransform t;
        t.scale(sx, sy);
        t.translate(-r.x(), -r.y());
        pixmapPath = t.map(path);
    }
    hide();
    Q_EMIT selected(globalRect, pixmapPath);
    close();
}

void CaptureOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    paintBackground(painter);
    const bool freehand = m_mode == QLatin1String("freehand");
    QRect sel;
    if (m_mode == QLatin1String("fixed") && rect().contains(m_mouse))
        sel = fixedRect(m_mouse);
    else if (m_mode == QLatin1String("window") && !m_dragging && m_hoverWindow)
        sel = *m_hoverWindow;
    else if (!m_current.isEmpty())
        sel = m_current;

    QPainterPath dim;
    dim.addRect(QRectF(rect()));
    if (freehand && !m_path.isEmpty()) {
        dim = dim.subtracted(m_path);
    } else if (!sel.isEmpty()) {
        QPainterPath hole;
        hole.addRect(QRectF(sel));
        dim = dim.subtracted(hole);
    }
    painter.fillPath(dim, kDim);

    painter.setRenderHint(QPainter::Antialiasing, freehand);
    painter.setPen(QPen(kAccent, 2));
    painter.setBrush(Qt::NoBrush);
    if (freehand && !m_path.isEmpty()) {
        painter.drawPath(m_path);
    } else if (!sel.isEmpty()) {
        painter.drawRect(sel.adjusted(0, 0, -1, -1));
        const QRect g = rectToGlobal(sel);
        const QString label = QStringLiteral("%1 x %2").arg(g.width()).arg(g.height());
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        const int tw = painter.fontMetrics().horizontalAdvance(label) + 12;
        QRect lr(sel.left(), sel.top() - 22, tw, 20);
        if (lr.top() < 0)
            lr.moveTop(sel.top() + 2);
        painter.fillRect(lr, QColor(20, 20, 20, 210));
        painter.setPen(Qt::white);
        painter.drawText(lr, Qt::AlignCenter, label);
    }

    if (!m_dragging
        && (m_mode == QLatin1String("region") || m_mode == QLatin1String("window") || freehand)) {
        painter.setPen(QPen(QColor(255, 255, 255, 120), 1, Qt::DashLine));
        painter.drawLine(0, m_mouse.y(), width(), m_mouse.y());
        painter.drawLine(m_mouse.x(), 0, m_mouse.x(), height());
    }

    if (m_mode != QLatin1String("fixed"))
        drawMagnifier(painter);
    if (m_showHint)
        drawHint(painter, hint(m_mode));
}

} // namespace mm::capture
