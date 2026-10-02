#include "editor/Ribbon.h"

#include "ui/Icons.h"

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace mm::editor {

namespace {

QString ribbonQss()
{
    return QStringLiteral(R"(
#RibbonTop { background: %1; }
#RibbonTop QToolButton { background: transparent; border: none; padding: 2px; }
#RibbonTop QToolButton:hover { background: rgba(255,255,255,40); }
QTabBar#RibbonTabs { background: transparent; }
QTabBar#RibbonTabs::tab {
    color: white; background: transparent; padding: 5px 16px; margin: 0; border: none;
    min-width: 30px;
}
QTabBar#RibbonTabs::tab:hover { background: rgba(255,255,255,40); }
QTabBar#RibbonTabs::tab:selected { background: #f3f3f3; color: %1; }
#RibbonBody { background: #f3f3f3; border-bottom: 1px solid #d5d5d5; }
#RibbonBody QToolButton {
    border: 1px solid transparent; border-radius: 2px; padding: 2px; background: transparent; color: #333;
}
#RibbonBody QToolButton:hover { background: #dcebfc; border-color: #a9cdf5; }
#RibbonBody QToolButton:checked { background: #c5dcf7; border-color: #88b4e6; }
#RibbonBody QToolButton:disabled { color: #a0a0a0; }
#RibbonBody QToolButton::menu-button { border: none; background: transparent; width: 10px; }
#RibbonBody QToolButton::menu-button:hover { border-left: 1px solid #a9cdf5; }
QLabel#RibbonGroupTitle { color: #666; font-size: 11px; }
)")
        .arg(QLatin1String(kAccent));
}

} // namespace

// ---------------------------------------------------------------- RibbonButton

RibbonButton::RibbonButton(const QString &text, const QString &iconName, bool large, QMenu *menu, bool checkable)
{
    init(text, icons::icon(iconName), large, menu, checkable);
}

RibbonButton::RibbonButton(const QString &text, const QIcon &icon, bool large, QMenu *menu, bool checkable)
{
    init(text, icon, large, menu, checkable);
}

void RibbonButton::init(const QString &text, const QIcon &icon, bool large, QMenu *menu, bool checkable)
{
    setText(text);
    setIcon(icon);
    setCheckable(checkable);
    setAutoRaise(true);
    if (large) {
        setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setIconSize(QSize(30, 30));
        setMinimumWidth(menu && !checkable ? 60 : 44);
        setFixedHeight(66);
    } else {
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setIconSize(QSize(16, 16));
        setFixedHeight(22);
    }
    if (menu) {
        setMenu(menu);
        setPopupMode(checkable ? QToolButton::MenuButtonPopup : QToolButton::InstantPopup);
    }
}

// ---------------------------------------------------------------- RibbonGroup

RibbonGroup::RibbonGroup(const QString &title)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *body = new QVBoxLayout();
    body->setContentsMargins(4, 2, 4, 2);
    body->setSpacing(0);
    m_content = new QHBoxLayout();
    m_content->setSpacing(2);
    body->addLayout(m_content, 1);
    auto *label = new QLabel(title);
    label->setObjectName(QStringLiteral("RibbonGroupTitle"));
    label->setAlignment(Qt::AlignCenter);
    body->addWidget(label);
    outer->addLayout(body);
    auto *sep = new QFrame();
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet(QStringLiteral("color: #d5d5d5;"));
    outer->addWidget(sep);
}

QWidget *RibbonGroup::add(QWidget *widget)
{
    m_content->addWidget(widget);
    return widget;
}

void RibbonGroup::addColumn(const QList<QWidget *> &widgets)
{
    auto *col = new QVBoxLayout();
    col->setSpacing(1);
    for (QWidget *w : widgets)
        col->addWidget(w);
    col->addStretch(1);
    m_content->addLayout(col);
}

// ---------------------------------------------------------------- RibbonPage

RibbonPage::RibbonPage()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 0);
    m_layout->setSpacing(0);
}

RibbonGroup *RibbonPage::addGroup(RibbonGroup *group)
{
    m_layout->addWidget(group);
    return group;
}

void RibbonPage::finish()
{
    if (!m_stretchAdded) {
        m_layout->addStretch(1);
        m_stretchAdded = true;
    }
}

// ---------------------------------------------------------------- ColorSwatch

ColorSwatch::ColorSwatch(const QString &label, const QColor &color)
    : m_color(color)
    , m_label(label)
{
    setFixedSize(44, 66);
    setCheckable(true);
}

void ColorSwatch::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void ColorSwatch::paintEvent(QPaintEvent *event)
{
    QToolButton::paintEvent(event);
    QPainter p(this);
    const QRect r = rect().adjusted(7, 6, -7, -26);
    p.setPen(QPen(QColor(QStringLiteral("#8a8a8a"))));
    p.setBrush(m_color);
    p.drawRect(r);
    p.setPen(isEnabled() ? QColor(QStringLiteral("#333")) : QColor(QStringLiteral("#a0a0a0")));
    p.drawText(rect().adjusted(0, 0, 0, -6), Qt::AlignBottom | Qt::AlignHCenter, m_label);
}

// ---------------------------------------------------------------- PaletteCell

PaletteCell::PaletteCell(const QColor &color)
    : m_color(color)
{
    setFixedSize(18, 18);
    setAutoRaise(false);
    setStyleSheet(QStringLiteral("border: none;"));
}

void PaletteCell::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void PaletteCell::mousePressEvent(QMouseEvent *event)
{
    if (m_color.isValid() && isEnabled())
        Q_EMIT picked(m_color, event->button() != Qt::RightButton);
}

void PaletteCell::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(QColor(QStringLiteral("#b0b0b0")));
    p.setBrush(m_color.isValid() ? m_color : QColor(QStringLiteral("#f3f3f3")));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_color.isValid()) {
        p.setPen(QColor(Qt::white));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
}

// ---------------------------------------------------------------- PaletteGrid

const QStringList &paletteColors()
{
    static const QStringList colors = {
        QStringLiteral("#000000"), QStringLiteral("#7f7f7f"), QStringLiteral("#880015"), QStringLiteral("#ed1c24"),
        QStringLiteral("#ff7f27"), QStringLiteral("#fff200"), QStringLiteral("#22b14c"), QStringLiteral("#00a2e8"),
        QStringLiteral("#3f48cc"), QStringLiteral("#a349a4"), QStringLiteral("#ffffff"), QStringLiteral("#c3c3c3"),
        QStringLiteral("#b97a57"), QStringLiteral("#ffaec9"), QStringLiteral("#ffc90e"), QStringLiteral("#efe4b0"),
        QStringLiteral("#b5e61d"), QStringLiteral("#99d9ea"), QStringLiteral("#7092be"), QStringLiteral("#c8bfe7"),
    };
    return colors;
}

PaletteGrid::PaletteGrid(const QStringList &custom)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(2, 4, 2, 2);
    grid->setSpacing(3);
    const QStringList &colors = paletteColors();
    for (int i = 0; i < colors.size(); ++i) {
        auto *cell = new PaletteCell(QColor(colors.at(i)));
        connect(cell, &PaletteCell::picked, this, &PaletteGrid::picked);
        grid->addWidget(cell, i / 10, i % 10);
    }
    for (int i = 0; i < 10; ++i) {
        auto *cell = new PaletteCell();
        connect(cell, &PaletteCell::picked, this, &PaletteGrid::picked);
        grid->addWidget(cell, 2, i);
        m_customCells.append(cell);
    }
    setCustom(custom);
}

void PaletteGrid::setCustom(const QStringList &colors)
{
    for (int i = 0; i < m_customCells.size(); ++i)
        m_customCells.at(i)->setColor(i < colors.size() ? QColor(colors.at(i)) : QColor());
}

// ---------------------------------------------------------------- Ribbon

Ribbon::Ribbon()
{
    setStyleSheet(ribbonQss());
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *top = new QWidget();
    top->setObjectName(QStringLiteral("RibbonTop"));
    top->setAttribute(Qt::WA_StyledBackground, true);
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(4, 2, 4, 0);
    topLayout->setSpacing(2);
    m_quick = new QHBoxLayout();
    m_quick->setSpacing(0);
    topLayout->addLayout(m_quick);
    topLayout->addSpacing(10);
    m_tabs = new QTabBar();
    m_tabs->setObjectName(QStringLiteral("RibbonTabs"));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    topLayout->addWidget(m_tabs);
    topLayout->addStretch(1);
    m_extra = new QHBoxLayout();
    topLayout->addLayout(m_extra);
    root->addWidget(top);

    m_body = new QWidget();
    m_body->setObjectName(QStringLiteral("RibbonBody"));
    m_body->setAttribute(Qt::WA_StyledBackground, true);
    auto *bodyLayout = new QHBoxLayout(m_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget();
    m_stack->setFixedHeight(92);
    bodyLayout->addWidget(m_stack, 1);
    m_collapseBtn = new QToolButton();
    m_collapseBtn->setText(QStringLiteral("︿"));
    m_collapseBtn->setToolTip(QStringLiteral("리본 축소/확장"));
    connect(m_collapseBtn, &QToolButton::clicked, this, &Ribbon::toggleCollapsed);
    bodyLayout->addWidget(m_collapseBtn, 0, Qt::AlignBottom);
    root->addWidget(m_body);

    m_tabs->addTab(QStringLiteral("파일"));
    connect(m_tabs, &QTabBar::currentChanged, this, &Ribbon::onTab);
}

QToolButton *Ribbon::addQuick(QAction *action)
{
    auto *btn = new QToolButton();
    btn->setDefaultAction(action);
    btn->setIconSize(QSize(16, 16));
    btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_quick->addWidget(btn);
    return btn;
}

void Ribbon::addPage(const QString &title, RibbonPage *page)
{
    page->finish();
    m_tabs->addTab(title);
    m_stack->addWidget(page);
    if (m_tabs->count() == 2)
        m_tabs->setCurrentIndex(1);
}

void Ribbon::onTab(int index)
{
    if (index == 0) {
        m_tabs->blockSignals(true);
        m_tabs->setCurrentIndex(m_lastIndex);
        m_tabs->blockSignals(false);
        Q_EMIT fileClicked();
        return;
    }
    m_lastIndex = index;
    m_stack->setCurrentIndex(index - 1);
    if (m_collapsed)
        toggleCollapsed();
}

void Ribbon::toggleCollapsed()
{
    m_collapsed = !m_collapsed;
    m_stack->setVisible(!m_collapsed);
    m_collapseBtn->setText(m_collapsed ? QStringLiteral("﹀") : QStringLiteral("︿"));
}

void Ribbon::setBodyEnabled(bool enabled, const QList<QWidget *> &keep)
{
    for (int i = 0; i < m_stack->count(); ++i) {
        QWidget *page = m_stack->widget(i);
        const QList<QToolButton *> buttons = page->findChildren<QToolButton *>();
        for (QToolButton *btn : buttons)
            btn->setEnabled(enabled || keep.contains(btn));
        const QList<PaletteCell *> cells = page->findChildren<PaletteCell *>();
        for (PaletteCell *cell : cells)
            cell->setEnabled(enabled);
    }
}

QWidget *spacer()
{
    auto *w = new QWidget();
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return w;
}

} // namespace mm::editor
