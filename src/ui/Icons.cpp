#include "ui/Icons.h"

#include <QBrush>
#include <QFont>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QtMath>

#include <cmath>
#include <initializer_list>

namespace mm::icons {

namespace {

QColor hex(const char *name)
{
    return QColor(QString::fromLatin1(name));
}

const QColor kInk = hex("#3d3d3d");
const QColor kBlue = hex("#2f6db5");
const QColor kLightBlue = hex("#9cc3ea");
const QColor kRed = hex("#d9452b");
const QColor kOrange = hex("#e8892c");

using PaintFn = void (*)(QPainter &);

void setPen(QPainter &p, const QColor &color = kInk, qreal width = 3.0)
{
    QPen pen(color, width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
}

void arrowHead(QPainter &p, const QPointF &tip, qreal angle, qreal size = 10.0)
{
    const qreal a1 = angle + qDegreesToRadians(150.0);
    const qreal a2 = angle - qDegreesToRadians(150.0);
    const QPolygonF head{
        tip,
        QPointF(tip.x() + size * std::cos(a1), tip.y() + size * std::sin(a1)),
        QPointF(tip.x() + size * std::cos(a2), tip.y() + size * std::sin(a2)),
    };
    p.drawPolygon(head);
}

QPolygonF poly(std::initializer_list<QPointF> points)
{
    return QPolygonF(QVector<QPointF>(points));
}

// --- App logo ---------------------------------------------------------------

void drawLogo(QPainter &p)
{
    const char *colors[] = {"#ff5f6d", "#ffa94d", "#38c2a4", "#4c7ef3"};
    p.translate(-3.5, 0);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 4; ++i) {
        QPainterPath path;
        const qreal x = 6 + i * 13;
        if (i % 2 == 0) {
            path.moveTo(x, 58);
            path.lineTo(x + 10, 58);
            path.lineTo(x + 20, 6);
            path.lineTo(x + 10, 6);
        } else {
            path.moveTo(x + 10, 58);
            path.lineTo(x + 20, 58);
            path.lineTo(x + 10, 6);
            path.lineTo(x, 6);
        }
        path.closeSubpath();
        p.setBrush(hex(colors[i]));
        p.drawPath(path);
    }
}

// --- File / common ----------------------------------------------------------

void drawNew(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    QPainterPath path;
    path.moveTo(16, 6);
    path.lineTo(38, 6);
    path.lineTo(50, 18);
    path.lineTo(50, 58);
    path.lineTo(16, 58);
    path.closeSubpath();
    p.drawPath(path);
    p.drawPolyline(poly({QPointF(38, 6), QPointF(38, 18), QPointF(50, 18)}));
}

void drawOpen(QPainter &p)
{
    setPen(p, hex("#b27b16"));
    p.setBrush(hex("#f6c95c"));
    p.drawRoundedRect(QRectF(6, 14, 46, 38), 3, 3);
    p.setBrush(hex("#fbe19c"));
    QPainterPath path;
    path.moveTo(14, 26);
    path.lineTo(58, 26);
    path.lineTo(50, 52);
    path.lineTo(6, 52);
    path.closeSubpath();
    p.drawPath(path);
    setPen(p, kBlue, 3.5);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(30, 30, 18, 18), 30 * 16, 260 * 16);
}

void drawSave(QPainter &p)
{
    setPen(p, kBlue);
    p.setBrush(hex("#dbe8f7"));
    p.drawRoundedRect(QRectF(8, 8, 48, 48), 4, 4);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(18, 8, 28, 16));
    p.drawRect(QRectF(16, 34, 32, 22));
}

void drawSaveAs(QPainter &p)
{
    drawSave(p);
    setPen(p, kOrange, 4);
    p.drawLine(QPointF(40, 56), QPointF(58, 38));
}

void drawPrint(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(18, 6, 28, 18));
    p.setBrush(hex("#c9c9c9"));
    p.drawRoundedRect(QRectF(6, 22, 52, 24), 4, 4);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(18, 38, 28, 20));
}

void drawUndo(QPainter &p)
{
    setPen(p, kBlue, 4);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(16, 26);
    path.cubicTo(30, 12, 54, 18, 54, 38);
    p.drawPath(path);
    p.setBrush(kBlue);
    arrowHead(p, QPointF(12, 30), qDegreesToRadians(150.0), 14);
}

void drawRedo(QPainter &p)
{
    p.translate(64, 0);
    p.scale(-1, 1);
    drawUndo(p);
}

void drawClose(QPainter &p)
{
    setPen(p, kInk, 4);
    p.drawLine(QPointF(16, 16), QPointF(48, 48));
    p.drawLine(QPointF(48, 16), QPointF(16, 48));
}

void drawBack(QPainter &p)
{
    setPen(p, QColor(Qt::white), 3.5);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(5, 5, 54, 54));
    setPen(p, QColor(Qt::white), 5);
    p.drawLine(QPointF(18, 32), QPointF(46, 32));
    p.drawPolyline(poly({QPointF(30, 20), QPointF(18, 32), QPointF(30, 44)}));
}

void drawHeart(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(hex("#e2603f"));
    QPainterPath path;
    path.moveTo(32, 56);
    path.cubicTo(4, 36, 4, 10, 22, 10);
    path.cubicTo(28, 10, 32, 16, 32, 20);
    path.cubicTo(32, 16, 36, 10, 42, 10);
    path.cubicTo(60, 10, 60, 36, 32, 56);
    p.drawPath(path);
}

void drawOptions(QPainter &p)
{
    setPen(p, kInk, 3);
    p.setBrush(hex("#d8d8d8"));
    QPainterPath path;
    for (int i = 0; i < 16; ++i) {
        const qreal ang = qDegreesToRadians(i * 22.5);
        const qreal r = i % 2 == 0 ? 26 : 20;
        const QPointF pt(32 + r * std::cos(ang), 32 + r * std::sin(ang));
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }
    path.closeSubpath();
    p.drawPath(path);
    p.setBrush(QColor(Qt::white));
    p.drawEllipse(QPointF(32, 32), 8, 8);
}

void drawInfo(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kBlue);
    p.drawEllipse(QRectF(6, 6, 52, 52));
    p.setPen(QColor(Qt::white));
    QFont f;
    f.setPixelSize(36);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(6, 6, 52, 52), Qt::AlignCenter, QStringLiteral("?"));
}

void drawUpdate(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kBlue);
    p.drawEllipse(QRectF(6, 6, 52, 52));
    setPen(p, QColor(Qt::white), 5);
    p.drawLine(QPointF(32, 16), QPointF(32, 44));
    p.drawPolyline(poly({QPointF(20, 34), QPointF(32, 46), QPointF(44, 34)}));
}

void drawShare(QPainter &p)
{
    setPen(p, kBlue, 3.5);
    p.setBrush(kLightBlue);
    for (const QPointF &c : {QPointF(46, 14), QPointF(16, 32), QPointF(46, 50)})
        p.drawEllipse(c, 8, 8);
    p.drawLine(QPointF(23, 28), QPointF(39, 18));
    p.drawLine(QPointF(23, 36), QPointF(39, 46));
}

void drawThumbnail(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    for (const QPointF &pt : {QPointF(8, 8), QPointF(34, 8), QPointF(8, 34), QPointF(34, 34)})
        p.drawRect(QRectF(pt.x(), pt.y(), 22, 22));
}

void drawEmail(QPainter &p)
{
    setPen(p, kBlue);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(6, 14, 52, 36));
    p.drawPolyline(poly({QPointF(6, 14), QPointF(32, 36), QPointF(58, 14)}));
}

void drawFtp(QPainter &p)
{
    setPen(p, kBlue);
    p.setBrush(kLightBlue);
    p.drawEllipse(QRectF(6, 6, 52, 52));
    p.drawEllipse(QRectF(20, 6, 24, 52));
    p.drawLine(QPointF(6, 32), QPointF(58, 32));
}

void drawProgram(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(6, 10, 52, 44));
    p.setBrush(kBlue);
    p.drawRect(QRectF(6, 10, 52, 10));
    setPen(p, kOrange, 4);
    p.drawLine(QPointF(22, 44), QPointF(42, 30));
    p.setBrush(kOrange);
    arrowHead(p, QPointF(44, 28), qDegreesToRadians(-35.0), 10);
}

// --- Clipboard --------------------------------------------------------------

void drawPaste(QPainter &p)
{
    setPen(p, hex("#8a6a2e"));
    p.setBrush(hex("#e9c88a"));
    p.drawRoundedRect(QRectF(10, 10, 40, 48), 3, 3);
    p.setBrush(hex("#9a9a9a"));
    p.drawRoundedRect(QRectF(22, 5, 16, 10), 2, 2);
    setPen(p);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(28, 26, 28, 32));
}

void drawCut(QPainter &p)
{
    setPen(p, kInk, 3);
    p.drawLine(QPointF(20, 6), QPointF(42, 42));
    p.drawLine(QPointF(44, 6), QPointF(22, 42));
    p.setBrush(Qt::NoBrush);
    setPen(p, kRed, 4);
    p.drawEllipse(QPointF(18, 48), 9, 9);
    p.drawEllipse(QPointF(46, 48), 9, 9);
}

void drawCopy(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(8, 6, 30, 38));
    p.drawRect(QRectF(26, 20, 30, 38));
}

// --- Image ------------------------------------------------------------------

void drawEffect(QPainter &p)
{
    setPen(p, kInk, 4);
    p.drawLine(QPointF(10, 54), QPointF(40, 24));
    p.setPen(Qt::NoPen);
    p.setBrush(kOrange);
    struct Dot { qreal cx, cy, r; };
    for (const Dot &d : {Dot{46, 14, 5}, Dot{54, 28, 3.5}, Dot{34, 10, 3}, Dot{52, 44, 2.5}})
        p.drawEllipse(QPointF(d.cx, d.cy), d.r, d.r);
}

void drawResize(QPainter &p)
{
    setPen(p, kInk, 2.5);
    QPen pen = p.pen();
    pen.setStyle(Qt::DashLine);
    p.setPen(pen);
    p.drawRect(QRectF(8, 8, 48, 48));
    setPen(p, kBlue, 3);
    p.setBrush(kLightBlue);
    p.drawRect(QRectF(8, 26, 30, 30));
    p.drawLine(QPointF(30, 34), QPointF(52, 12));
    p.setBrush(kBlue);
    arrowHead(p, QPointF(54, 10), qDegreesToRadians(-45.0), 10);
}

void drawRotate(QPainter &p)
{
    setPen(p);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(8, 24, 26, 32));
    setPen(p, kBlue, 4);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(20, 8, 36, 36), 0, 150 * 16);
    p.setBrush(kBlue);
    arrowHead(p, QPointF(56, 28), qDegreesToRadians(90.0), 12);
}

void drawCrop(QPainter &p)
{
    setPen(p, kInk, 4);
    p.drawPolyline(poly({QPointF(16, 4), QPointF(16, 48), QPointF(60, 48)}));
    p.drawPolyline(poly({QPointF(4, 16), QPointF(48, 16), QPointF(48, 60)}));
}

// --- Tools ------------------------------------------------------------------

void drawMove(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(QColor(Qt::white));
    p.drawPolygon(poly({QPointF(8, 6), QPointF(8, 44), QPointF(18, 35), QPointF(26, 52),
                        QPointF(32, 49), QPointF(24, 33), QPointF(37, 33)}));
    setPen(p, kBlue, 3);
    p.drawLine(QPointF(40, 46), QPointF(60, 46));
    p.drawLine(QPointF(50, 36), QPointF(50, 58));
}

void drawSelect(QPainter &p)
{
    p.setPen(QPen(kInk, 3, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(8, 12, 48, 40));
}

void drawDraw(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(hex("#f2b134"));
    QPainterPath path;
    path.moveTo(44, 6);
    path.lineTo(58, 20);
    path.lineTo(24, 54);
    path.lineTo(10, 40);
    path.closeSubpath();
    p.drawPath(path);
    p.setBrush(kInk);
    p.drawPolygon(poly({QPointF(10, 40), QPointF(24, 54), QPointF(6, 58)}));
}

void drawHighlighter(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 230, 0, 170));
    p.drawRect(QRectF(4, 40, 56, 16));
    setPen(p, kInk, 2.5);
    p.setBrush(hex("#ffe600"));
    QPainterPath path;
    path.moveTo(40, 6);
    path.lineTo(56, 22);
    path.lineTo(32, 46);
    path.lineTo(16, 30);
    path.closeSubpath();
    p.drawPath(path);
}

void drawEraser(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(hex("#f1a3b3"));
    QPainterPath path;
    path.moveTo(36, 8);
    path.lineTo(58, 30);
    path.lineTo(32, 56);
    path.lineTo(10, 34);
    path.closeSubpath();
    p.drawPath(path);
    p.drawLine(QPointF(22, 22), QPointF(44, 44));
}

void drawFill(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(QColor(Qt::white));
    QPainterPath path;
    path.moveTo(26, 8);
    path.lineTo(48, 30);
    path.lineTo(28, 50);
    path.lineTo(6, 28);
    path.closeSubpath();
    p.drawPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(kBlue);
    QPainterPath drop;
    drop.moveTo(54, 36);
    drop.cubicTo(60, 46, 60, 54, 54, 54);
    drop.cubicTo(48, 54, 48, 46, 54, 36);
    p.drawPath(drop);
}

void drawTextIcon(QPainter &p)
{
    QFont f(QStringLiteral("Serif"));
    f.setPixelSize(54);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter, QStringLiteral("T"));
}

void drawStamp(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(hex("#9a9a9a"));
    p.drawEllipse(QRectF(22, 4, 20, 20));
    p.drawRect(QRectF(27, 22, 10, 14));
    p.setBrush(hex("#c9c9c9"));
    p.drawRoundedRect(QRectF(8, 34, 48, 14), 3, 3);
    p.drawRect(QRectF(12, 48, 40, 8));
}

void drawNumberStamp(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kRed);
    p.drawEllipse(QRectF(6, 6, 52, 52));
    p.setPen(QColor(Qt::white));
    QFont f;
    f.setPixelSize(34);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(6, 6, 52, 52), Qt::AlignCenter, QStringLiteral("1"));
}

void drawMoveCursorOnly(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(QColor(Qt::white));
    p.drawPolygon(poly({QPointF(16, 6), QPointF(16, 52), QPointF(28, 41), QPointF(37, 60),
                        QPointF(44, 57), QPointF(35, 38), QPointF(51, 38)}));
}

void drawShape(QPainter &p)
{
    setPen(p, kBlue, 3);
    p.setBrush(kLightBlue);
    p.drawRect(QRectF(6, 22, 32, 32));
    p.setBrush(QColor(255, 255, 255, 200));
    p.drawEllipse(QRectF(26, 6, 32, 32));
}

void drawRectIcon(QPainter &p)
{
    setPen(p, kInk, 3.5);
    p.drawRect(QRectF(8, 14, 48, 36));
}

void drawRoundedRectIcon(QPainter &p)
{
    setPen(p, kInk, 3.5);
    p.drawRoundedRect(QRectF(8, 14, 48, 36), 10, 10);
}

void drawEllipseIcon(QPainter &p)
{
    setPen(p, kInk, 3.5);
    p.drawEllipse(QRectF(6, 14, 52, 36));
}

void drawLineIcon(QPainter &p)
{
    setPen(p, kInk, 4);
    p.drawLine(QPointF(8, 56), QPointF(56, 8));
}

void drawArrow(QPainter &p)
{
    setPen(p, kRed, 4.5);
    p.drawLine(QPointF(8, 56), QPointF(50, 14));
    p.setBrush(kRed);
    arrowHead(p, QPointF(56, 8), qDegreesToRadians(-45.0), 16);
}

void drawBalloon(QPainter &p)
{
    setPen(p, kInk, 3);
    p.setBrush(QColor(Qt::white));
    QPainterPath path;
    path.addRoundedRect(QRectF(6, 8, 52, 34), 8, 8);
    QPainterPath tail;
    tail.moveTo(18, 40);
    tail.lineTo(14, 58);
    tail.lineTo(30, 40);
    tail.closeSubpath();
    p.drawPath(path.united(tail));
}

void drawMosaic(QPainter &p)
{
    p.setPen(Qt::NoPen);
    const char *shades[] = {"#555", "#999", "#ccc", "#777"};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            p.setBrush(hex(shades[(i + j * 3) % 4]));
            p.drawRect(QRectF(8 + i * 12, 8 + j * 12, 12, 12));
        }
    }
}

void drawBlur(QPainter &p)
{
    QLinearGradient grad(0, 0, 64, 64);
    grad.setColorAt(0, hex("#ffffff"));
    grad.setColorAt(1, hex("#5c8fd1"));
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(grad));
    p.drawEllipse(QRectF(8, 8, 48, 48));
}

void drawEyedropper(QPainter &p)
{
    setPen(p, kInk, 2.5);
    p.setBrush(hex("#5a5a5a"));
    QPainterPath path;
    path.moveTo(44, 6);
    path.lineTo(58, 20);
    path.lineTo(50, 26);
    path.lineTo(38, 14);
    path.closeSubpath();
    p.drawPath(path);
    p.setBrush(kLightBlue);
    QPainterPath tube;
    tube.moveTo(40, 18);
    tube.lineTo(46, 24);
    tube.lineTo(18, 52);
    tube.lineTo(10, 54);
    tube.lineTo(12, 46);
    tube.closeSubpath();
    p.drawPath(tube);
}

void drawSwap(QPainter &p)
{
    setPen(p, kInk, 3);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(12, 12, 40, 40), 30 * 16, 120 * 16);
    p.drawArc(QRectF(12, 12, 40, 40), 210 * 16, 120 * 16);
    p.setBrush(kInk);
    arrowHead(p, QPointF(49, 22), qDegreesToRadians(60.0), 9);
    arrowHead(p, QPointF(15, 42), qDegreesToRadians(240.0), 9);
}

void drawResetColors(QPainter &p)
{
    setPen(p, kInk, 2);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(24, 24, 28, 28));
    p.setBrush(QColor(Qt::black));
    p.drawRect(QRectF(12, 12, 28, 28));
}

void drawMoreColors(QPainter &p)
{
    p.setPen(Qt::NoPen);
    const char *colors[] = {"#e74c3c", "#f1c40f", "#2ecc71", "#3498db", "#9b59b6", "#95a5a6"};
    for (int i = 0; i < 6; ++i) {
        const qreal ang = qDegreesToRadians(qreal(i * 60 - 90));
        p.setBrush(hex(colors[i]));
        p.drawEllipse(QPointF(32 + 18 * std::cos(ang), 32 + 18 * std::sin(ang)), 8, 8);
    }
}

void drawLineWidth(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(kInk);
    qreal y = 10;
    for (qreal h : {2, 4, 6, 9}) {
        p.drawRect(QRectF(6, y, 52, h));
        y += h + 8;
    }
}

void drawMagnifier(QPainter &p)
{
    setPen(p, kBlue, 4);
    p.setBrush(QColor(255, 255, 255, 200));
    p.drawEllipse(QPointF(38, 24), 18, 18);
    setPen(p, kBlue, 7);
    p.drawLine(QPointF(25, 37), QPointF(8, 56));
}

void drawZoomIn(QPainter &p)
{
    drawMagnifier(p);
    setPen(p, kBlue, 4);
    p.drawLine(QPointF(30, 24), QPointF(46, 24));
    p.drawLine(QPointF(38, 16), QPointF(38, 32));
}

void drawZoomOut(QPainter &p)
{
    drawMagnifier(p);
    setPen(p, kBlue, 4);
    p.drawLine(QPointF(30, 24), QPointF(46, 24));
}

void drawZoomFit(QPainter &p)
{
    setPen(p, kInk, 3);
    p.drawPolyline(poly({QPointF(8, 22), QPointF(8, 8), QPointF(22, 8)}));
    p.drawPolyline(poly({QPointF(42, 8), QPointF(56, 8), QPointF(56, 22)}));
    p.drawPolyline(poly({QPointF(56, 42), QPointF(56, 56), QPointF(42, 56)}));
    p.drawPolyline(poly({QPointF(22, 56), QPointF(8, 56), QPointF(8, 42)}));
    p.setBrush(kLightBlue);
    p.drawRect(QRectF(18, 18, 28, 28));
}

void drawZoom100(QPainter &p)
{
    QFont f;
    f.setPixelSize(22);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kInk);
    p.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter, QStringLiteral("1:1"));
}

void drawGrid(QPainter &p)
{
    setPen(p, kInk, 2);
    for (int i = 0; i < 5; ++i) {
        const qreal v = 8 + i * 12;
        p.drawLine(QPointF(v, 8), QPointF(v, 56));
        p.drawLine(QPointF(8, v), QPointF(56, v));
    }
}

// --- Screen capture ---------------------------------------------------------

void drawCapFullscreen(QPainter &p)
{
    setPen(p, kBlue, 3);
    p.setBrush(QColor(Qt::white));
    p.drawRoundedRect(QRectF(4, 8, 56, 38), 3, 3);
    p.drawLine(QPointF(32, 46), QPointF(32, 54));
    p.drawLine(QPointF(18, 56), QPointF(46, 56));
}

void drawCapWindow(QPainter &p)
{
    setPen(p, kBlue, 3);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(6, 10, 52, 44));
    p.setBrush(kBlue);
    p.drawRect(QRectF(6, 10, 52, 10));
}

void drawCapControl(QPainter &p)
{
    drawCapWindow(p);
    setPen(p, kInk, 3);
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawEllipse(QPointF(42, 42), 9, 9);
    p.drawLine(QPointF(48, 48), QPointF(58, 58));
}

void drawCapScroll(QPainter &p)
{
    setPen(p, kBlue, 3);
    p.setBrush(QColor(Qt::white));
    p.drawRect(QRectF(8, 6, 48, 52));
    for (qreal y : {16, 24, 32})
        p.drawLine(QPointF(16, y), QPointF(48, y));
    p.setBrush(kBlue);
    setPen(p, kBlue, 4);
    p.drawLine(QPointF(32, 30), QPointF(32, 50));
    arrowHead(p, QPointF(32, 54), qDegreesToRadians(90.0), 10);
}

void drawCapRegion(QPainter &p)
{
    p.setPen(QPen(kInk, 3, Qt::DashLine));
    p.drawRect(QRectF(6, 6, 40, 40));
    setPen(p, kBlue, 4);
    p.drawLine(QPointF(46, 40), QPointF(46, 60));
    p.drawLine(QPointF(36, 50), QPointF(56, 50));
}

void drawCapFixed(QPainter &p)
{
    p.setPen(QPen(kRed, 3, Qt::DashLine));
    p.drawRect(QRectF(10, 10, 44, 44));
    p.setPen(Qt::NoPen);
    p.setBrush(kRed);
    for (const QPointF &pt : {QPointF(10, 10), QPointF(54, 10), QPointF(10, 54), QPointF(54, 54),
                              QPointF(32, 10), QPointF(32, 54), QPointF(10, 32), QPointF(54, 32)})
        p.drawRect(QRectF(pt.x() - 3, pt.y() - 3, 6, 6));
}

void drawCapFreehand(QPainter &p)
{
    setPen(p, kInk, 3);
    QPainterPath path;
    path.moveTo(14, 40);
    path.cubicTo(0, 30, 10, 10, 26, 14);
    path.cubicTo(32, 2, 56, 6, 54, 22);
    path.cubicTo(62, 30, 52, 44, 40, 40);
    path.cubicTo(30, 48, 20, 46, 14, 40);
    p.drawPath(path);
    setPen(p, kRed, 4);
    p.drawLine(QPointF(48, 42), QPointF(48, 60));
    p.drawLine(QPointF(39, 51), QPointF(57, 51));
}

void drawCapRepeat(QPainter &p)
{
    setPen(p, kBlue, 5);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(10, 10, 44, 44), 100 * 16, 300 * 16);
    p.setBrush(kBlue);
    setPen(p, kBlue, 2);
    arrowHead(p, QPointF(26, 8), qDegreesToRadians(180.0), 14);
}

void drawCapDelay(QPainter &p)
{
    setPen(p, kInk, 3);
    p.setBrush(QColor(Qt::white));
    p.drawEllipse(QRectF(8, 8, 48, 48));
    setPen(p, kBlue, 4);
    p.drawLine(QPointF(32, 32), QPointF(32, 16));
    p.drawLine(QPointF(32, 32), QPointF(44, 38));
}

// --- Graphic tools ----------------------------------------------------------

void drawToolPalette(QPainter &p)
{
    p.setPen(QPen(kInk, 1.5));
    const char *colors[] = {"#d94a4a", "#4aa3d9", "#6fbf4a", "#e8a33c", "#8b5fbf",
                            "#4a6ad9", "#bf4a8b", "#3cbfa0", "#d9d94a"};
    for (int i = 0; i < 9; ++i) {
        p.setBrush(hex(colors[i]));
        p.drawRect(QRectF(8 + (i % 3) * 16, 8 + (i / 3) * 16, 14, 14));
    }
}

void drawToolRuler(QPainter &p)
{
    setPen(p, kOrange, 3);
    p.setBrush(hex("#fff3dc"));
    QPainterPath path;
    path.moveTo(8, 6);
    path.lineTo(20, 6);
    path.lineTo(20, 46);
    path.lineTo(58, 46);
    path.lineTo(58, 58);
    path.lineTo(8, 58);
    path.closeSubpath();
    p.drawPath(path);
    setPen(p, kOrange, 2);
    for (int y = 12; y < 44; y += 6)
        p.drawLine(QPointF(8, y), QPointF(14, y));
    for (int x = 26; x < 58; x += 6)
        p.drawLine(QPointF(x, 58), QPointF(x, 52));
}

void drawToolCrosshair(QPainter &p)
{
    setPen(p, kInk, 3);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(32, 32), 20, 20);
    p.drawLine(QPointF(32, 4), QPointF(32, 60));
    p.drawLine(QPointF(4, 32), QPointF(60, 32));
    p.setPen(Qt::NoPen);
    p.setBrush(kRed);
    p.drawEllipse(QPointF(32, 32), 5, 5);
}

void drawToolProtractor(QPainter &p)
{
    setPen(p, kBlue, 3);
    p.setBrush(kLightBlue);
    QPainterPath outer;
    outer.moveTo(4, 48);
    outer.arcTo(QRectF(4, 16, 56, 64), 180, -180);
    outer.closeSubpath();
    p.drawPath(outer);
    p.setBrush(QColor(Qt::white));
    QPainterPath inner;
    inner.moveTo(20, 48);
    inner.arcTo(QRectF(20, 34, 24, 28), 180, -180);
    inner.closeSubpath();
    p.drawPath(inner);
}

void drawToolWhiteboard(QPainter &p)
{
    setPen(p, kInk, 2);
    p.setBrush(kBlue);
    QPainterPath path;
    path.moveTo(48, 6);
    path.lineTo(58, 16);
    path.lineTo(22, 52);
    path.lineTo(12, 42);
    path.closeSubpath();
    p.drawPath(path);
    p.setBrush(kInk);
    p.drawPolygon(poly({QPointF(12, 42), QPointF(22, 52), QPointF(6, 58)}));
}

struct Registry
{
    QHash<QString, PaintFn> painters;
    QStringList order;
};

const Registry &registry()
{
    static const Registry reg = [] {
        Registry r;
        const auto add = [&r](const char *name, PaintFn fn) {
            const QString key = QString::fromLatin1(name);
            r.painters.insert(key, fn);
            r.order.append(key);
        };
        add("logo", drawLogo);
        add("new", drawNew);
        add("open", drawOpen);
        add("save", drawSave);
        add("save_as", drawSaveAs);
        add("print", drawPrint);
        add("undo", drawUndo);
        add("redo", drawRedo);
        add("close", drawClose);
        add("back", drawBack);
        add("heart", drawHeart);
        add("options", drawOptions);
        add("info", drawInfo);
        add("update", drawUpdate);
        add("share", drawShare);
        add("thumbnail", drawThumbnail);
        add("email", drawEmail);
        add("ftp", drawFtp);
        add("program", drawProgram);
        add("clipboard_copy", drawCopy);
        add("paste", drawPaste);
        add("cut", drawCut);
        add("copy", drawCopy);
        add("effect", drawEffect);
        add("resize", drawResize);
        add("rotate", drawRotate);
        add("crop", drawCrop);
        add("move", drawMove);
        add("select", drawSelect);
        add("draw", drawDraw);
        add("highlighter", drawHighlighter);
        add("eraser", drawEraser);
        add("fill", drawFill);
        add("text", drawTextIcon);
        add("stamp", drawStamp);
        add("number_stamp", drawNumberStamp);
        add("cursor_stamp", drawMoveCursorOnly);
        add("shape", drawShape);
        add("rect", drawRectIcon);
        add("rounded_rect", drawRoundedRectIcon);
        add("ellipse", drawEllipseIcon);
        add("line", drawLineIcon);
        add("arrow", drawArrow);
        add("balloon", drawBalloon);
        add("mosaic", drawMosaic);
        add("blur", drawBlur);
        add("eyedropper", drawEyedropper);
        add("swap", drawSwap);
        add("reset_colors", drawResetColors);
        add("more_colors", drawMoreColors);
        add("line_width", drawLineWidth);
        add("zoom_in", drawZoomIn);
        add("zoom_out", drawZoomOut);
        add("zoom_fit", drawZoomFit);
        add("zoom_100", drawZoom100);
        add("grid", drawGrid);
        add("cap_fullscreen", drawCapFullscreen);
        add("cap_window", drawCapWindow);
        add("cap_control", drawCapControl);
        add("cap_scroll", drawCapScroll);
        add("cap_region", drawCapRegion);
        add("cap_fixed", drawCapFixed);
        add("cap_freehand", drawCapFreehand);
        add("cap_repeat", drawCapRepeat);
        add("cap_delay", drawCapDelay);
        add("tool_color_picker", drawEyedropper);
        add("tool_palette", drawToolPalette);
        add("tool_magnifier", drawMagnifier);
        add("tool_ruler", drawToolRuler);
        add("tool_crosshair", drawToolCrosshair);
        add("tool_protractor", drawToolProtractor);
        add("tool_whiteboard", drawToolWhiteboard);
        add("tool_editor", drawLogo);
        add("tray", drawLogo);
        return r;
    }();
    return reg;
}

} // namespace

QPixmap render(const QString &name, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 64.0, size / 64.0);
    const PaintFn fn = registry().painters.value(name, nullptr);
    if (!fn) {
        setPen(painter);
        painter.drawRect(QRectF(12, 12, 40, 40));
    } else {
        fn(painter);
    }
    painter.end();
    return pm;
}

QIcon icon(const QString &name)
{
    static QHash<QString, QIcon> cache;
    const auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return it.value();
    QIcon ic;
    for (int size : {16, 24, 32, 48, 64})
        ic.addPixmap(render(name, size));
    cache.insert(name, ic);
    return ic;
}

bool has(const QString &name)
{
    return registry().painters.contains(name);
}

QStringList names()
{
    return registry().order;
}

} // namespace mm::icons
