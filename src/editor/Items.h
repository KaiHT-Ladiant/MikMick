#pragma once

#include <QColor>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

// Editable objects placed on the canvas above the raster background.
namespace mm::editor {

inline const QGraphicsItem::GraphicsItemFlags kObjectFlags =
    QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges;

// "rect", "rounded_rect", "ellipse", "line", "arrow", "balloon"
const QStringList &shapeKinds();
QString shapeLabel(const QString &kind);

class ShapeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    // An invalid fill colour means "no fill".
    ShapeItem(const QString &kind, const QPointF &p1, const QPointF &p2, const QColor &color, int width,
              const QColor &fill = QColor());

    int type() const override { return Type; }
    QString kind() const { return m_kind; }
    QPointF p1() const { return m_p1; }
    QPointF p2() const { return m_p2; }
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    int width() const { return m_width; }
    QColor fill() const { return m_fill; }
    void setEnd(const QPointF &p2);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    QRectF rect() const;
    double headSize() const;
    QPainterPath balloonPath() const;

    QString m_kind;
    QPointF m_p1;
    QPointF m_p2;
    QColor m_color;
    int m_width;
    QColor m_fill;
};

// Pencil / highlighter / eraser freehand stroke.
class StrokeItem : public QGraphicsPathItem
{
public:
    enum { Type = UserType + 2 };

    StrokeItem(const QPointF &start, const QColor &color, int width, bool highlighter = false);

    int type() const override { return Type; }
    bool isHighlighter() const { return m_highlighter; }
    void addPoint(const QPointF &p);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    bool m_highlighter;
};

class TextItem : public QGraphicsTextItem
{
public:
    enum { Type = UserType + 3 };

    TextItem(const QString &text, const QFont &font, const QColor &color);

    int type() const override { return Type; }
    void startEditing();

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    // Leaving edit mode with only whitespace removes the item from the scene.
    void focusOutEvent(QFocusEvent *event) override;
};

// Number stamp ("number") or mouse cursor stamp ("cursor").
class StampItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 4 };

    StampItem(const QString &kind, int number, const QColor &color, int size = 28);

    int type() const override { return Type; }
    QString kind() const { return m_kind; }
    int number() const { return m_number; }
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    int size() const { return m_size; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    QString m_kind;
    int m_number;
    QColor m_color;
    int m_size;
};

class PixmapObject : public QGraphicsPixmapItem
{
public:
    enum { Type = UserType + 5 };

    explicit PixmapObject(const QPixmap &pixmap);

    int type() const override { return Type; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
};

} // namespace mm::editor
