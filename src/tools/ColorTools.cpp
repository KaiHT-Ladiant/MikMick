#include "tools/ColorTools.h"

#include "ui/Icons.h"

#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace mm::tools {

namespace {

constexpr int kMaxCustomColors = 16;

QString hex2(int v)
{
    return QStringLiteral("%1").arg(v, 2, 16, QLatin1Char('0')).toUpper();
}

void applyCustomColors(const QStringList &colors)
{
    for (int i = 0; i < qMin(kMaxCustomColors, int(colors.size())); ++i)
        QColorDialog::setCustomColor(i, QColor(colors.at(i)));
}

} // namespace

const QList<QPair<QString, QString>> &colorFormats()
{
    static const QList<QPair<QString, QString>> formats{
        {QStringLiteral("html"), QStringLiteral("HTML (#RRGGBB)")},
        {QStringLiteral("hex"), QStringLiteral("HEX (RRGGBB)")},
        {QStringLiteral("rgb"), QStringLiteral("RGB (r, g, b)")},
        {QStringLiteral("rgba"), QStringLiteral("RGBA (r, g, b, a)")},
        {QStringLiteral("hsb"), QStringLiteral("HSB (h, s, b)")},
        {QStringLiteral("cpp"), QStringLiteral("C++ (0x00BBGGRR)")},
        {QStringLiteral("delphi"), QStringLiteral("Delphi ($00BBGGRR)")},
    };
    return formats;
}

QString formatColor(const QColor &c, const QString &format)
{
    const int r = c.red();
    const int g = c.green();
    const int b = c.blue();
    if (format == QLatin1String("hex"))
        return hex2(r) + hex2(g) + hex2(b);
    if (format == QLatin1String("rgb"))
        return QStringLiteral("rgb(%1, %2, %3)").arg(r).arg(g).arg(b);
    if (format == QLatin1String("rgba"))
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(r).arg(g).arg(b).arg(c.alphaF(), 0, 'f', 2);
    if (format == QLatin1String("hsb")) {
        int h = 0, s = 0, v = 0, a = 0;
        c.getHsv(&h, &s, &v, &a);
        return QStringLiteral("hsb(%1, %2%, %3%)")
            .arg(qMax(0, h))
            .arg(qRound(s / 2.55))
            .arg(qRound(v / 2.55));
    }
    if (format == QLatin1String("cpp"))
        return QStringLiteral("0x00") + hex2(b) + hex2(g) + hex2(r);
    if (format == QLatin1String("delphi"))
        return QStringLiteral("$00") + hex2(b) + hex2(g) + hex2(r);
    return QStringLiteral("#") + hex2(r) + hex2(g) + hex2(b);
}

// ---------------------------------------------------------------------------

ColorPickOverlay::ColorPickOverlay(const capture::Snapshot &snap, int zoom)
    : FrozenOverlay(snap, true, zoom)
{
}

void ColorPickOverlay::pick(const QPoint &pos)
{
    const QColor c = colorAt(pos);
    hide();
    Q_EMIT picked(c);
    close();
}

void ColorPickOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_mouse = event->position().toPoint();
    update();
}

void ColorPickOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        pick(event->position().toPoint());
    else
        cancel();
}

void ColorPickOverlay::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        pick(m_mouse);
        return;
    default:
        FrozenOverlay::keyPressEvent(event);
    }
}

void ColorPickOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintBackground(p);
    drawMagnifier(p);
    drawHint(p, QStringLiteral("클릭하여 색상을 추출하세요.  방향키: 1px 이동   ESC: 취소"));
}

// ---------------------------------------------------------------------------

class ColorSwatch : public QLabel
{
public:
    ColorSwatch()
    {
        setFixedSize(64, 64);
    }

    void setColor(const QColor &c)
    {
        m_color = c;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), m_color);
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QColor m_color{Qt::white};
};

ColorToolWindow::ColorToolWindow(Config &config, bool paletteMode, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_config(config)
{
    setWindowTitle(paletteMode ? QStringLiteral("색상 팔레트") : QStringLiteral("색상 추출 도구"));
    setWindowIcon(icons::icon(paletteMode ? QStringLiteral("tool_palette") : QStringLiteral("tool_color_picker")));
    setAttribute(Qt::WA_DeleteOnClose);
    auto *root = new QVBoxLayout(this);

    m_dialog = new QColorDialog(this);
    m_dialog->setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog
                         | QColorDialog::ShowAlphaChannel);
    m_dialog->setWindowFlags(Qt::Widget);
    applyCustomColors(config.list(QStringLiteral("palette"), QStringLiteral("custom")));
    root->addWidget(m_dialog);

    auto *row = new QHBoxLayout;
    m_swatch = new ColorSwatch;
    row->addWidget(m_swatch);
    auto *col = new QVBoxLayout;
    m_format = new QComboBox;
    for (const auto &f : colorFormats())
        m_format->addItem(f.second, f.first);
    m_format->setCurrentIndex(
        qMax(0, m_format->findData(config.str(QStringLiteral("palette"), QStringLiteral("format"), QStringLiteral("html")))));
    col->addWidget(m_format);
    m_code = new QLineEdit;
    m_code->setReadOnly(true);
    col->addWidget(m_code);
    row->addLayout(col, 1);
    root->addLayout(row);

    auto *buttons = new QHBoxLayout;
    auto *pickButton = new QPushButton(icons::icon(QStringLiteral("eyedropper")), QStringLiteral("화면에서 추출"));
    connect(pickButton, &QPushButton::clicked, this, &ColorToolWindow::pickAgain);
    auto *copyButton = new QPushButton(icons::icon(QStringLiteral("copy")), QStringLiteral("코드 복사"));
    connect(copyButton, &QPushButton::clicked, this, &ColorToolWindow::copyCode);
    auto *saveButton = new QPushButton(QStringLiteral("팔레트에 저장"));
    connect(saveButton, &QPushButton::clicked, this, &ColorToolWindow::saveCustom);
    auto *closeButton = new QPushButton(QStringLiteral("닫기"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    buttons->addWidget(pickButton);
    buttons->addWidget(copyButton);
    buttons->addWidget(saveButton);
    buttons->addStretch(1);
    buttons->addWidget(closeButton);
    root->addLayout(buttons);

    connect(m_dialog, &QColorDialog::currentColorChanged, this, &ColorToolWindow::updateColor);
    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged), this, &ColorToolWindow::formatChanged);
    updateColor(m_dialog->currentColor());
}

void ColorToolWindow::setColor(const QColor &c)
{
    m_dialog->setCurrentColor(c);
    updateColor(c);
}

QColor ColorToolWindow::color() const
{
    return m_dialog->currentColor();
}

QString ColorToolWindow::codeText() const
{
    return m_code->text();
}

QString ColorToolWindow::currentFormat() const
{
    return m_format->currentData().toString();
}

void ColorToolWindow::formatChanged()
{
    m_config.set(QStringLiteral("palette"), QStringLiteral("format"), QJsonValue(currentFormat()));
    m_config.save();
    updateColor(m_dialog->currentColor());
}

void ColorToolWindow::updateColor(const QColor &c)
{
    m_swatch->setColor(c);
    m_code->setText(formatColor(c, currentFormat()));
}

void ColorToolWindow::copyCode()
{
    QGuiApplication::clipboard()->setText(m_code->text());
}

void ColorToolWindow::saveCustom()
{
    const QString name = m_dialog->currentColor().name();
    QStringList custom = m_config.list(QStringLiteral("palette"), QStringLiteral("custom"));
    custom.removeAll(name);
    custom.prepend(name);
    custom = custom.mid(0, kMaxCustomColors);
    m_config.set(QStringLiteral("palette"), QStringLiteral("custom"), custom);
    m_config.save();
    applyCustomColors(custom);
}

void ColorToolWindow::bringToFront()
{
    show();
    raise();
    activateWindow();
}

void ColorToolWindow::bringToFront(const QPoint &pos)
{
    move(pos);
    bringToFront();
}

} // namespace mm::tools
