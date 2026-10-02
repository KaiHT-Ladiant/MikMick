#include "editor/Items.h"

#include <QBrush>
#include <QGraphicsScene>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPen>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>

#include <algorithm>
#include <cmath>

namespace mm::editor {

namespace {

void drawSelection(QPainter *painter, const QRectF &rect)
{
    painter->save();
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(47, 140, 255), 1, Qt::DashLine));
    painter->drawRect(rect);
    painter->restore();
}

bool optionSelected(const QStyleOptionGraphicsItem *option)
{
    return option && (option->state & QStyle::State_Selected);
}

constexpr double kPi = 3.14159265358979323846;

} // namespace

const QStringList &shapeKinds()
{
    static const QStringList kinds = {QStringLiteral("rect"), QStringLiteral("rounded_rect"), QStringLiteral("ellipse"),
                                      QStringLiteral("line"), QStringLiteral("arrow"), QStringLiteral("balloon")};
    return kinds;
}

QString shapeLabel(const QString &kind)
{
    if (kind == QLatin1String("rect"))
        return QStringLiteral("사각형");
    if (kind == QLatin1String("rounded_rect"))
        return QStringLiteral("둥근 사각형");
    if (kind == QLatin1String("ellipse"))
        return QStringLiteral("타원");
    if (kind == QLatin1String("line"))
        return QStringLiteral("직선");
    if (kind == QLatin1String("arrow"))
        return QStringLiteral("화살표");
    if (kind == QLatin1String("balloon"))
        return QStringLiteral("말풍선");
    return kind;
}

// ---------------------------------------------------------------- ShapeItem

ShapeItem::ShapeItem(const QString &kind, const QPointF &p1, const QPointF &p2, const QColor &color, int width,
                     const QColor &fill)
    : m_kind(kind)
    , m_p1(p1)
    , m_p2(p2)
    , m_color(color)
    , m_width(width)
    , m_fill(fill)
{
    setFlags(kObjectFlags);
}

void ShapeItem::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void ShapeItem::setEnd(const QPointF &p2)
{
    prepareGeometryChange();
    m_p2 = p2;
    update();
}

QRectF ShapeItem::rect() const
{
    return QRectF(m_p1, m_p2).normalized();
}

double ShapeItem::headSize() const
{
    return std::max(10.0, m_width * 4.0);
}

QRectF ShapeItem::boundingRect() const
{
    const double m = m_width + (m_kind == QLatin1String("arrow") ? headSize() : 2.0) + 4.0;
    QRectF r = rect();
    if (m_kind == QLatin1String("balloon"))
        r = r.adjusted(0, 0, 0, r.height() * 0.4);
    return r.adjusted(-m, -m, m, m);
}

QPainterPath ShapeItem::shape() const
{
    QPainterPath path;
    if (m_kind == QLatin1String("line") || m_kind == QLatin1String("arrow")) {
        path.moveTo(m_p1);
        path.lineTo(m_p2);
        QPainterPathStroker stroker;
        stroker.setWidth(std::max(8, m_width + 6));
        return stroker.createStroke(path);
    }
    path.addRect(boundingRect());
    return path;
}

QPainterPath ShapeItem::balloonPath() const
{
    const QRectF r = rect();
    QPainterPath body;
    body.addRoundedRect(r, std::min(16.0, r.width() / 4), std::min(16.0, r.height() / 4));
    QPainterPath tail;
    const double tx = r.left() + r.width() * 0.25;
    tail.moveTo(tx, r.bottom() - 1);
    tail.lineTo(tx - r.width() * 0.05, r.bottom() + r.height() * 0.4);
    tail.lineTo(tx + r.width() * 0.18, r.bottom() - 1);
    tail.closeSubpath();
    return body.united(tail);
}

void ShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    QPen pen(m_color, m_width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    if (m_fill.isValid())
        painter->setBrush(QBrush(m_fill));
    else
        painter->setBrush(Qt::NoBrush);
    const QRectF r = rect();
    if (m_kind == QLatin1String("rect")) {
        painter->drawRect(r);
    } else if (m_kind == QLatin1String("rounded_rect")) {
        const double rad = std::min(18.0, std::min(r.width(), r.height()) / 4);
        painter->drawRoundedRect(r, rad, rad);
    } else if (m_kind == QLatin1String("ellipse")) {
        painter->drawEllipse(r);
    } else if (m_kind == QLatin1String("balloon")) {
        painter->drawPath(balloonPath());
    } else if (m_kind == QLatin1String("line")) {
        painter->drawLine(m_p1, m_p2);
    } else if (m_kind == QLatin1String("arrow")) {
        const QLineF line(m_p1, m_p2);
        const double head = headSize();
        if (line.length() > 1) {
            const double ang = std::atan2(line.dy(), line.dx());
            const QPointF base(m_p2.x() - head * 0.8 * std::cos(ang), m_p2.y() - head * 0.8 * std::sin(ang));
            painter->drawLine(m_p1, base);
            const double spread = 155.0 * kPi / 180.0;
            const double a1 = ang + spread;
            const double a2 = ang - spread;
            const QPolygonF poly({m_p2, QPointF(m_p2.x() + head * std::cos(a1), m_p2.y() + head * std::sin(a1)),
                                  QPointF(m_p2.x() + head * std::cos(a2), m_p2.y() + head * std::sin(a2))});
            painter->setPen(QPen(m_color, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(m_color);
            painter->drawPolygon(poly);
        }
    }
    if (optionSelected(option))
        drawSelection(painter, boundingRect());
}

// ---------------------------------------------------------------- StrokeItem

StrokeItem::StrokeItem(const QPointF &start, const QColor &color, int width, bool highlighter)
    : m_highlighter(highlighter)
{
    setPath(QPainterPath(start));
    QPen pen(color, width);
    pen.setCapStyle(highlighter ? Qt::SquareCap : Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    setPen(pen);
    setFlags(kObjectFlags);
}

void StrokeItem::addPoint(const QPointF &p)
{
    QPainterPath p2 = path();
    p2.lineTo(p);
    setPath(p2);
}

void StrokeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    if (m_highlighter)
        painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    painter->setPen(pen());
    painter->setBrush(Qt::NoBrush);
    const QPainterPath p = path();
    if (p.elementCount() <= 1)
        painter->drawPoint(p.currentPosition());
    else
        painter->drawPath(p);
    if (optionSelected(option)) {
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawSelection(painter, boundingRect());
    }
}

// ---------------------------------------------------------------- TextItem

TextItem::TextItem(const QString &text, const QFont &font, const QColor &color)
    : QGraphicsTextItem(text)
{
    setFont(font);
    setDefaultTextColor(color);
    setFlags(kObjectFlags);
}

void TextItem::startEditing()
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus(Qt::MouseFocusReason);
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::Document);
    setTextCursor(cursor);
}

void TextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    startEditing();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void TextItem::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);
    QGraphicsTextItem::focusOutEvent(event);
    if (toPlainText().trimmed().isEmpty() && scene())
        scene()->removeItem(this);
}

// ---------------------------------------------------------------- StampItem

StampItem::StampItem(const QString &kind, int number, const QColor &color, int size)
    : m_kind(kind)
    , m_number(number)
    , m_color(color)
    , m_size(size)
{
    setFlags(kObjectFlags);
}

void StampItem::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QRectF StampItem::boundingRect() const
{
    const double s = m_size;
    if (m_kind == QLatin1String("cursor"))
        return QRectF(-2, -2, s * 0.75 + 4, s * 1.1 + 4);
    return QRectF(-s / 2 - 2, -s / 2 - 2, s + 4, s + 4);
}

void StampItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    const double s = m_size;
    if (m_kind == QLatin1String("cursor")) {
        const double k = s / 20.0;
        const QPolygonF poly({QPointF(0, 0), QPointF(0, 17 * k), QPointF(4 * k, 13 * k), QPointF(7 * k, 20 * k),
                              QPointF(10 * k, 19 * k), QPointF(7 * k, 12 * k), QPointF(12 * k, 12 * k)});
        painter->setPen(QPen(QColor(Qt::black), 1.2));
        painter->setBrush(QColor(Qt::white));
        painter->drawPolygon(poly);
    } else {
        painter->setPen(QPen(m_color.darker(130), 1.5));
        painter->setBrush(m_color);
        painter->drawEllipse(QRectF(-s / 2, -s / 2, s, s));
        QFont font;
        font.setBold(true);
        font.setPixelSize(std::max(1, int(s * (m_number < 10 ? 0.6 : 0.5))));
        painter->setFont(font);
        const double lum = 0.299 * m_color.red() + 0.587 * m_color.green() + 0.114 * m_color.blue();
        painter->setPen(lum > 170 ? QColor(Qt::black) : QColor(Qt::white));
        painter->drawText(QRectF(-s / 2, -s / 2, s, s), Qt::AlignCenter, QString::number(m_number));
    }
    if (optionSelected(option))
        drawSelection(painter, boundingRect());
}

// ---------------------------------------------------------------- PixmapObject

PixmapObject::PixmapObject(const QPixmap &pixmap)
    : QGraphicsPixmapItem(pixmap)
{
    setFlags(kObjectFlags);
    setTransformationMode(Qt::SmoothTransformation);
}

void PixmapObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QStyleOptionGraphicsItem opt(*option);
    const bool selected = opt.state & QStyle::State_Selected;
    opt.state &= ~QStyle::State_Selected;
    QGraphicsPixmapItem::paint(painter, &opt, widget);
    if (selected)
        drawSelection(painter, boundingRect());
}

} // namespace mm::editor
