#include "tools/ScreenTools.h"

#include "core/System.h"
#include "ui/Icons.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFont>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>
#include <utility>

namespace mm::tools {

namespace {

double pyMod(double a, double m)
{
    const double r = std::fmod(a, m);
    return r < 0 ? r + m : r;
}

// Degrees counter-clockwise from the +x axis (screen y points down).
double legAngle(const QPointF &v, const QPointF &a)
{
    return qRadiansToDegrees(std::atan2(-(a.y() - v.y()), a.x() - v.x()));
}

const QList<QPair<QString, QString>> &rulerUnits()
{
    static const QList<QPair<QString, QString>> units{
        {QStringLiteral("px"), QStringLiteral("픽셀")},
        {QStringLiteral("in"), QStringLiteral("인치")},
        {QStringLiteral("cm"), QStringLiteral("센티미터")},
    };
    return units;
}

} // namespace

// ---------------------------------------------------------------------------

Magnifier::Magnifier(Config &config, std::optional<capture::Snapshot> snapshot, QWidget *parent)
    : QWidget(parent)
    , m_snapshot(std::move(snapshot))
{
    const int zoom = config.num(QStringLiteral("capture"), QStringLiteral("magnifier_zoom"), 6);
    m_zoom = zoom ? zoom : 4;
    setWindowTitle(QStringLiteral("돋보기"));
    setWindowIcon(icons::icon(QStringLiteral("tool_magnifier")));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(360, 260);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Magnifier::tick);
    m_timer->start(40);
    setToolTip(QStringLiteral("휠: 배율 변경   스페이스: 따라가기 일시정지   ESC: 닫기"));
}

void Magnifier::tick()
{
    if (!m_follow)
        return;
    const QPoint pos = QCursor::pos();
    if (frameGeometry().contains(pos))
        return;
    const int w = qMax(1, width() / m_zoom);
    const int h = qMax(1, height() / m_zoom);
    const QRect rect(pos.x() - w / 2, pos.y() - h / 2, w, h);
    const QPixmap pm = m_snapshot ? m_snapshot->crop(rect) : capture::grabLive(rect);
    if (!pm.isNull()) {
        m_frame = pm;
        update();
    }
}

void Magnifier::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x20, 0x20, 0x20));
    if (!m_frame.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawPixmap(rect(), m_frame);
    }
    const QPoint c = rect().center();
    p.setPen(QPen(QColor(255, 60, 60, 200), 1));
    p.drawLine(c.x() - 10, c.y(), c.x() + 10, c.y());
    p.drawLine(c.x(), c.y() - 10, c.x(), c.y() + 10);
    p.setPen(Qt::white);
    p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignBottom | Qt::AlignRight, QStringLiteral("%1x").arg(m_zoom));
}

void Magnifier::wheelEvent(QWheelEvent *event)
{
    m_zoom = qBound(2, m_zoom + (event->angleDelta().y() > 0 ? 1 : -1), 32);
    update();
}

void Magnifier::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else if (event->key() == Qt::Key_Space)
        m_follow = !m_follow;
    else
        QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------

Ruler::Ruler(Config &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_unit(config.str(QStringLiteral("ruler"), QStringLiteral("unit"), QStringLiteral("px")))
    , m_dpi(config.num(QStringLiteral("ruler"), QStringLiteral("dpi"), 96))
{
    if (m_unit != QLatin1String("in") && m_unit != QLatin1String("cm"))
        m_unit = QStringLiteral("px");
    if (m_dpi <= 0)
        m_dpi = 96;
    setWindowTitle(QStringLiteral("눈금자"));
    setWindowIcon(icons::icon(QStringLiteral("tool_ruler")));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    resize(800, 60);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_timer->start(50);
    setToolTip(QStringLiteral("드래그: 이동   끝 부분 드래그: 길이 조절   더블클릭: 방향 전환   우클릭: 메뉴"));
}

double Ruler::pixelsPerUnit() const
{
    if (m_unit == QLatin1String("in"))
        return m_dpi;
    if (m_unit == QLatin1String("cm"))
        return m_dpi / 2.54;
    return 1.0;
}

void Ruler::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(255, 243, 200, 225));
    p.setPen(QColor(0xa0, 0x70, 0x20));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    const int length = m_horizontal ? width() : height();
    const double ppu = pixelsPerUnit();
    const bool px = m_unit == QLatin1String("px");
    int minor = 2, mid = 10, major = 50;
    double stepPx = 1.0;
    if (m_unit == QLatin1String("in")) {
        minor = 1;
        mid = 4;
        major = 16;
        stepPx = ppu / 16;
    } else if (!px) {
        minor = 1;
        mid = 5;
        major = 10;
        stepPx = ppu / 10;
    }
    QFont f;
    f.setPixelSize(10);
    p.setFont(f);
    for (int i = 0;; ++i) {
        const double pos = i * stepPx;
        if (pos > length)
            break;
        if (i % minor != 0)
            continue;
        const int size = (i % major == 0) ? 18 : (i % mid == 0) ? 11 : 5;
        const int x = int(pos);
        if (m_horizontal)
            p.drawLine(x, 0, x, size);
        else
            p.drawLine(0, x, size, x);
        if (i % major == 0 && i != 0) {
            const QString label = QString::number(px ? i : i / major);
            if (m_horizontal)
                p.drawText(x + 2, 30, label);
            else
                p.drawText(22, x + 4, label);
        }
    }
    const QPoint cur = mapFromGlobal(QCursor::pos());
    p.setPen(QPen(QColor(0xd9, 0x45, 0x2b), 1));
    const double val = (m_horizontal ? cur.x() : cur.y()) / ppu;
    const QString text = px ? QString::number(val, 'f', 0) + QStringLiteral(" px")
                            : QString(QString::number(val, 'f', 2) + QLatin1Char(' ') + m_unit);
    if (m_horizontal && cur.x() >= 0 && cur.x() <= width()) {
        p.drawLine(cur.x(), 0, cur.x(), height());
        p.drawText(QRect(0, 0, width() - 6, height() - 4), Qt::AlignRight | Qt::AlignBottom, text);
    } else if (!m_horizontal && cur.y() >= 0 && cur.y() <= height()) {
        p.drawLine(0, cur.y(), width(), cur.y());
        p.drawText(QRect(0, 0, width() - 4, height() - 6), Qt::AlignRight | Qt::AlignBottom, text);
    }
}

bool Ruler::nearEnd(const QPoint &pos) const
{
    return m_horizontal ? (width() - pos.x() < 10) : (height() - pos.y() < 10);
}

void Ruler::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_resize = nearEnd(event->position().toPoint());
        m_drag = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void Ruler::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (!m_drag) {
        if (nearEnd(pos))
            setCursor(m_horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
        else
            setCursor(Qt::SizeAllCursor);
        return;
    }
    if (m_resize) {
        if (m_horizontal)
            resize(qMax(100, pos.x()), height());
        else
            resize(width(), qMax(100, pos.y()));
    } else {
        move(event->globalPosition().toPoint() - *m_drag);
    }
}

void Ruler::mouseReleaseEvent(QMouseEvent *)
{
    m_drag.reset();
    m_resize = false;
}

void Ruler::mouseDoubleClickEvent(QMouseEvent *)
{
    toggleOrientation();
}

void Ruler::toggleOrientation()
{
    m_horizontal = !m_horizontal;
    resize(height(), width());
}

void Ruler::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    connect(menu.addAction(QStringLiteral("가로/세로 전환")), &QAction::triggered, this, &Ruler::toggleOrientation);
    QMenu *unitMenu = menu.addMenu(QStringLiteral("단위"));
    auto *group = new QActionGroup(&menu);
    for (const auto &u : rulerUnits()) {
        auto *a = new QAction(u.second, &menu);
        a->setCheckable(true);
        a->setChecked(u.first == m_unit);
        const QString key = u.first;
        connect(a, &QAction::triggered, this, [this, key] { setUnit(key); });
        group->addAction(a);
        unitMenu->addAction(a);
    }
    connect(menu.addAction(QStringLiteral("DPI 설정... (%1)").arg(m_dpi)), &QAction::triggered, this, &Ruler::askDpi);
    menu.addSeparator();
    connect(menu.addAction(QStringLiteral("닫기")), &QAction::triggered, this, &QWidget::close);
    menu.exec(event->globalPos());
}

void Ruler::setUnit(const QString &unit)
{
    m_unit = unit;
    m_config.set(QStringLiteral("ruler"), QStringLiteral("unit"), QJsonValue(unit));
    m_config.save();
}

void Ruler::askDpi()
{
    bool ok = false;
    const int val = QInputDialog::getInt(this, QStringLiteral("DPI"), QStringLiteral("DPI:"), m_dpi, 30, 1200, 1, &ok);
    if (ok) {
        m_dpi = val;
        m_config.set(QStringLiteral("ruler"), QStringLiteral("dpi"), QJsonValue(val));
        m_config.save();
    }
}

void Ruler::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    QPoint d;
    switch (event->key()) {
    case Qt::Key_Left: d = QPoint(-step, 0); break;
    case Qt::Key_Right: d = QPoint(step, 0); break;
    case Qt::Key_Up: d = QPoint(0, -step); break;
    case Qt::Key_Down: d = QPoint(0, step); break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    move(pos() + d);
}

void Ruler::start()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect g = screen->availableGeometry();
        move(g.center().x() - width() / 2, g.center().y() - height() / 2);
    }
    show();
    raise();
    activateWindow();
}

// ---------------------------------------------------------------------------

CrosshairOverlay::CrosshairOverlay(const capture::Snapshot &snap, int zoom)
    : FrozenOverlay(snap, true, zoom)
{
}

void CrosshairOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_mouse = event->position().toPoint();
    update();
}

void CrosshairOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        cancel();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const QPoint p = event->position().toPoint();
        if (event->modifiers() & Qt::ControlModifier)
            m_marks.append(p);
        else
            m_origin = p;
        update();
    }
}

void CrosshairOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        const QPointF gp = toGlobal(QPointF(m_mouse));
        QGuiApplication::clipboard()->setText(QStringLiteral("%1, %2").arg(int(gp.x())).arg(int(gp.y())));
        return;
    }
    if (event->key() == Qt::Key_R) {
        m_origin.reset();
        m_marks.clear();
        update();
        return;
    }
    FrozenOverlay::keyPressEvent(event);
}

void CrosshairOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintBackground(p);
    p.fillRect(rect(), QColor(0, 0, 0, 40));
    p.setPen(QPen(QColor(255, 40, 40), 1));
    p.drawLine(0, m_mouse.y(), width(), m_mouse.y());
    p.drawLine(m_mouse.x(), 0, m_mouse.x(), height());
    QStringList extra;
    if (m_origin) {
        const QPoint o = *m_origin;
        p.setPen(QPen(QColor(47, 140, 255), 1, Qt::DashLine));
        p.drawLine(0, o.y(), width(), o.y());
        p.drawLine(o.x(), 0, o.x(), height());
        p.drawRect(QRect(o, m_mouse).normalized());
        const QPointF go = toGlobal(QPointF(o));
        const QPointF gm = toGlobal(QPointF(m_mouse));
        const int dx = int(gm.x() - go.x());
        const int dy = int(gm.y() - go.y());
        extra << QStringLiteral("상대: %1, %2  (거리 %3)").arg(dx).arg(dy).arg(std::hypot(dx, dy), 0, 'f', 1);
    }
    for (const QPoint &m : std::as_const(m_marks)) {
        p.setPen(QPen(QColor(255, 200, 0), 2));
        p.drawEllipse(m, 4, 4);
    }
    drawMagnifier(p, extra);
    drawHint(p, QStringLiteral("클릭: 기준점 지정   Ctrl+클릭: 표시   R: 초기화   Ctrl+C: 좌표 복사   ESC: 닫기"));
}

// ---------------------------------------------------------------------------

ProtractorOverlay::ProtractorOverlay(const capture::Snapshot &snap, int zoom)
    : FrozenOverlay(snap, false, zoom)
{
}

void ProtractorOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_mouse = event->position().toPoint();
    update();
}

void ProtractorOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (!m_points.isEmpty()) {
            m_points.removeLast();
            update();
        } else {
            cancel();
        }
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_points.size() >= 3)
            m_points.clear();
        m_points.append(event->position());
        update();
    }
}

void ProtractorOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintBackground(p);
    p.fillRect(rect(), QColor(0, 0, 0, 40));
    p.setRenderHint(QPainter::Antialiasing);
    QList<QPointF> pts = m_points;
    if (pts.size() < 3)
        pts.append(QPointF(m_mouse));
    const QPen pen(QColor(47, 140, 255), 2);
    p.setPen(pen);
    QFont f;
    f.setPixelSize(14);
    f.setBold(true);
    p.setFont(f);
    if (pts.size() >= 2) {
        const QPointF v = pts.at(0);
        p.drawLine(v, pts.at(1));
        const double a1 = legAngle(v, pts.at(1));
        if (pts.size() == 2) {
            p.setPen(QPen(QColor(255, 255, 255, 140), 1, Qt::DashLine));
            p.drawLine(v, QPointF(v.x() + 200, v.y()));
            drawLabel(p, v, QStringLiteral("%1°").arg(pyMod(a1, 360), 0, 'f', 1));
        }
        if (pts.size() >= 3) {
            p.setPen(pen);
            p.drawLine(v, pts.at(2));
            const double a2 = legAngle(v, pts.at(2));
            const double diff = pyMod(a2 - a1, 360);
            const double inner = diff <= 180 ? diff : 360 - diff;
            const double r = 40;
            p.setPen(QPen(QColor(255, 200, 0), 2));
            const double start = diff <= 180 ? a1 : a2;
            p.drawArc(QRectF(v.x() - r, v.y() - r, 2 * r, 2 * r), int(start * 16), int(inner * 16));
            drawLabel(p, v,
                      QStringLiteral("%1°  (반대 %2°)").arg(inner, 0, 'f', 1).arg(360 - inner, 0, 'f', 1));
        }
    }
    for (const QPointF &pt : std::as_const(m_points)) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 60, 60));
        p.drawEllipse(pt, 4, 4);
    }
    drawHint(p, QStringLiteral("1) 꼭짓점 클릭  2) 첫 번째 선 클릭  3) 두 번째 선 클릭   우클릭: 되돌리기   ESC: 닫기"));
}

void ProtractorOverlay::drawLabel(QPainter &p, const QPointF &v, const QString &text)
{
    const QRectF r(v.x() + 14, v.y() + 10, p.fontMetrics().horizontalAdvance(text) + 16, 26);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 210));
    p.drawRoundedRect(r, 5, 5);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, text);
}

bool needsSnapshotForLiveTools()
{
    return sys::isWayland();
}

} // namespace mm::tools
