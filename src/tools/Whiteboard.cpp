#include "tools/Whiteboard.h"

#include "core/System.h"
#include "ui/Icons.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QToolButton>

#include <cmath>
#include <utility>

namespace mm::tools {

namespace {

const QStringList &boardColors()
{
    static const QStringList colors{
        QStringLiteral("#ff3b30"), QStringLiteral("#ffcc00"), QStringLiteral("#34c759"),
        QStringLiteral("#007aff"), QStringLiteral("#000000"), QStringLiteral("#ffffff"),
    };
    return colors;
}

struct ToolDef
{
    const char *key;
    const char *icon;
    QString tip;
};

const QList<ToolDef> &boardTools()
{
    static const QList<ToolDef> tools{
        {"pen", "draw", QStringLiteral("펜")},
        {"highlighter", "highlighter", QStringLiteral("형광펜")},
        {"line", "line", QStringLiteral("직선")},
        {"arrow", "arrow", QStringLiteral("화살표")},
        {"rect", "rect", QStringLiteral("사각형")},
        {"ellipse", "ellipse", QStringLiteral("타원")},
        {"eraser", "eraser", QStringLiteral("지우개")},
    };
    return tools;
}

bool isFreehand(const QString &kind)
{
    return kind == QLatin1String("pen") || kind == QLatin1String("highlighter")
        || kind == QLatin1String("eraser");
}

} // namespace

void WhiteboardStroke::paint(QPainter &p) const
{
    p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const QPointF start = points.value(0);
    if (isFreehand(kind)) {
        if (kind == QLatin1String("eraser"))
            p.setCompositionMode(QPainter::CompositionMode_Clear);
        QPainterPath path(start);
        for (int i = 1; i < points.size(); ++i)
            path.lineTo(points.at(i));
        if (points.size() == 1)
            path.lineTo(start + QPointF(0.1, 0.1));
        p.drawPath(path);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    } else if (kind == QLatin1String("line")) {
        p.drawLine(start, end);
    } else if (kind == QLatin1String("arrow")) {
        p.drawLine(start, end);
        const double ang = std::atan2(end.y() - start.y(), end.x() - start.x());
        const double head = qMax(12, width * 4);
        const QPolygonF poly{
            end,
            QPointF(end.x() + head * std::cos(ang + 2.7), end.y() + head * std::sin(ang + 2.7)),
            QPointF(end.x() + head * std::cos(ang - 2.7), end.y() + head * std::sin(ang - 2.7)),
        };
        p.setBrush(color);
        p.drawPolygon(poly);
    } else if (kind == QLatin1String("rect")) {
        p.drawRect(QRectF(start, end).normalized());
    } else if (kind == QLatin1String("ellipse")) {
        p.drawEllipse(QRectF(start, end).normalized());
    }
}

// ---------------------------------------------------------------------------

Whiteboard::Whiteboard(std::optional<capture::Snapshot> snapshot, QWidget *parent)
    : QWidget(parent)
    , m_snapshot(std::move(snapshot))
    , m_background(m_snapshot ? QStringLiteral("screen") : QStringLiteral("transparent"))
    , m_color(boardColors().first())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    buildToolbar();
}

void Whiteboard::buildToolbar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("WbBar"));
    bar->setStyleSheet(QStringLiteral(
        "#WbBar { background: rgba(32,32,32,230); border-radius: 8px; }"
        "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; color: white; padding: 3px; }"
        "QToolButton:hover { background: rgba(255,255,255,40); }"
        "QToolButton:checked { background: rgba(47,140,255,160); }"
        "QLabel { color: #ddd; }"));
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(8, 4, 8, 4);
    lay->setSpacing(2);

    auto *group = new QButtonGroup(this);
    for (const ToolDef &t : boardTools()) {
        const QString key = QLatin1String(t.key);
        auto *b = new QToolButton;
        b->setIcon(icons::icon(QLatin1String(t.icon)));
        b->setToolTip(t.tip);
        b->setCheckable(true);
        b->setChecked(key == m_kind);
        connect(b, &QToolButton::clicked, this, [this, key] { m_kind = key; });
        group->addButton(b);
        lay->addWidget(b);
    }
    lay->addSpacing(8);

    auto *colorGroup = new QButtonGroup(this);
    for (int i = 0; i < boardColors().size(); ++i) {
        const QString c = boardColors().at(i);
        auto *b = new QToolButton;
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setFixedSize(22, 22);
        b->setStyleSheet(
            QStringLiteral("QToolButton { background: %1; border: 2px solid #555; border-radius: 11px; }"
                           "QToolButton:checked { border: 2px solid white; }")
                .arg(c));
        connect(b, &QToolButton::clicked, this, [this, c] { m_color = QColor(c); });
        colorGroup->addButton(b);
        lay->addWidget(b);
    }
    lay->addSpacing(8);

    auto *widthGroup = new QButtonGroup(this);
    for (int w : {2, 4, 8, 14}) {
        auto *b = new QToolButton;
        b->setText(QString::number(w));
        b->setCheckable(true);
        b->setChecked(w == m_penWidth);
        connect(b, &QToolButton::clicked, this, [this, w] { m_penWidth = w; });
        widthGroup->addButton(b);
        lay->addWidget(b);
    }
    lay->addSpacing(8);

    const QList<QPair<QString, QString>> backgrounds{
        {QStringLiteral("투명"), QStringLiteral("transparent")},
        {QStringLiteral("화면"), QStringLiteral("screen")},
        {QStringLiteral("흰색"), QStringLiteral("white")},
        {QStringLiteral("검정"), QStringLiteral("black")},
    };
    for (const auto &bg : backgrounds) {
        auto *b = new QToolButton;
        b->setText(bg.first);
        b->setToolTip(QStringLiteral("배경"));
        const QString mode = bg.second;
        connect(b, &QToolButton::clicked, this, [this, mode] { setBackground(mode); });
        lay->addWidget(b);
    }
    lay->addSpacing(8);

    auto *undoButton = new QToolButton;
    undoButton->setIcon(icons::icon(QStringLiteral("undo")));
    undoButton->setToolTip(QStringLiteral("실행 취소 (Ctrl+Z)"));
    connect(undoButton, &QToolButton::clicked, this, &Whiteboard::undo);
    lay->addWidget(undoButton);
    auto *redoButton = new QToolButton;
    redoButton->setIcon(icons::icon(QStringLiteral("redo")));
    redoButton->setToolTip(QStringLiteral("다시 실행 (Ctrl+Y)"));
    connect(redoButton, &QToolButton::clicked, this, &Whiteboard::redo);
    lay->addWidget(redoButton);

    auto *clearButton = new QToolButton;
    clearButton->setText(QStringLiteral("지우기"));
    connect(clearButton, &QToolButton::clicked, this, &Whiteboard::clear);
    lay->addWidget(clearButton);
    auto *closeButton = new QToolButton;
    closeButton->setText(QStringLiteral("닫기 (ESC)"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    lay->addWidget(closeButton);
    m_hint = new QLabel;
    lay->addWidget(m_hint);
    bar->adjustSize();
    m_bar = bar;
}

void Whiteboard::setBackground(const QString &mode)
{
    if (mode == QLatin1String("screen") && !m_snapshot)
        return;
    m_background = mode;
    update();
}

void Whiteboard::placeToolbar()
{
    m_bar->move((width() - m_bar->width()) / 2, 12);
}

void Whiteboard::start()
{
    if (sys::isWayland()) {
        showFullScreen();
    } else {
        setGeometry(m_snapshot ? m_snapshot->geometry : capture::virtualGeometry(true));
        show();
    }
    placeToolbar();
    raise();
    activateWindow();
    setFocus();
}

void Whiteboard::resizeEvent(QResizeEvent *event)
{
    placeToolbar();
    QWidget::resizeEvent(event);
}

void Whiteboard::undo()
{
    if (!m_strokes.isEmpty()) {
        m_redo.append(m_strokes.takeLast());
        update();
    }
}

void Whiteboard::redo()
{
    if (!m_redo.isEmpty()) {
        m_strokes.append(m_redo.takeLast());
        update();
    }
}

void Whiteboard::clear()
{
    m_strokes.clear();
    m_redo.clear();
    update();
}

void Whiteboard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        undo();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    WhiteboardStroke s;
    s.kind = m_kind;
    s.color = m_color;
    s.width = m_penWidth;
    if (m_kind == QLatin1String("highlighter")) {
        s.color.setAlpha(110);
        s.width = m_penWidth * 4;
    } else if (m_kind == QLatin1String("eraser")) {
        s.width = m_penWidth * 5;
    }
    s.points.append(event->position());
    s.end = event->position();
    m_current = s;
    m_redo.clear();
}

void Whiteboard::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_current)
        return;
    if (isFreehand(m_current->kind))
        m_current->points.append(event->position());
    else
        m_current->end = event->position();
    update();
}

void Whiteboard::mouseReleaseEvent(QMouseEvent *)
{
    if (m_current) {
        m_strokes.append(*m_current);
        m_current.reset();
        update();
    }
}

void Whiteboard::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    if (event->key() == Qt::Key_Escape)
        close();
    else if (ctrl && event->key() == Qt::Key_Z)
        undo();
    else if (ctrl && event->key() == Qt::Key_Y)
        redo();
    else if (event->key() == Qt::Key_Delete)
        clear();
    else
        QWidget::keyPressEvent(event);
}

void Whiteboard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_background == QLatin1String("screen") && m_snapshot) {
        p.drawPixmap(rect(), m_snapshot->pixmap);
    } else if (m_background == QLatin1String("white")) {
        p.fillRect(rect(), Qt::white);
    } else if (m_background == QLatin1String("black")) {
        p.fillRect(rect(), QColor(0x11, 0x11, 0x11));
    } else {
        // Fully transparent pixels let mouse input fall through on some compositors.
        p.fillRect(rect(), QColor(0, 0, 0, 1));
    }

    const qreal dpr = devicePixelRatioF();
    const QSize layerSize = size() * dpr;
    if (m_layer.size() != layerSize) {
        m_layer = QPixmap(layerSize);
        m_layer.setDevicePixelRatio(dpr);
    }
    m_layer.fill(Qt::transparent);
    QPainter lp(&m_layer);
    lp.setRenderHint(QPainter::Antialiasing);
    for (const WhiteboardStroke &s : std::as_const(m_strokes))
        s.paint(lp);
    if (m_current)
        m_current->paint(lp);
    lp.end();
    p.drawPixmap(0, 0, m_layer);
}

} // namespace mm::tools
