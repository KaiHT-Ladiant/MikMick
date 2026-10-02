#include "editor/Dialogs.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>

#include <algorithm>
#include <cmath>

namespace mm::editor {

namespace {

QDialogButtonBox *okCancel(QDialog *dlg)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("확인"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return buttons;
}

// Python's round(): halves go to the even neighbour.
int roundHalfEven(double v)
{
    return int(std::nearbyint(v));
}

} // namespace

// ---------------------------------------------------------------- ColorButton

ColorButton::ColorButton(const QColor &color, QWidget *parent)
    : QPushButton(parent)
    , m_color(color)
{
    setFixedSize(60, 24);
    connect(this, &QPushButton::clicked, this, &ColorButton::pick);
    refresh();
}

void ColorButton::setColor(const QColor &color)
{
    m_color = color;
    refresh();
}

void ColorButton::pick()
{
    const QColor c = QColorDialog::getColor(m_color, this, QStringLiteral("색 선택"), QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        m_color = c;
        refresh();
    }
}

void ColorButton::refresh()
{
    setStyleSheet(QStringLiteral("background: %1; border: 1px solid #888;").arg(m_color.name(QColor::HexArgb)));
}

// ---------------------------------------------------------------- ParamField

ParamField ParamField::integer(const QString &key, const QString &label, int def, int min, int max)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Int;
    f.def = def;
    f.min = min;
    f.max = max;
    return f;
}

ParamField ParamField::real(const QString &key, const QString &label, double def, double min, double max)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Float;
    f.def = def;
    f.min = min;
    f.max = max;
    return f;
}

ParamField ParamField::choice(const QString &key, const QString &label, const QString &def,
                              const QList<QPair<QString, QString>> &choices)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Choice;
    f.def = def;
    f.choices = choices;
    return f;
}

ParamField ParamField::text(const QString &key, const QString &label, const QString &def)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Text;
    f.def = def;
    return f;
}

ParamField ParamField::color(const QString &key, const QString &label, const QColor &def)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Color;
    f.def = def;
    return f;
}

ParamField ParamField::boolean(const QString &key, const QString &label, bool def)
{
    ParamField f;
    f.key = key;
    f.label = label;
    f.kind = Bool;
    f.def = def;
    return f;
}

// ---------------------------------------------------------------- ParamDialog

ParamDialog::ParamDialog(const QString &title, const QList<ParamField> &fields, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumWidth(340);
    auto *form = new QFormLayout(this);
    for (const ParamField &field : fields) {
        QWidget *widget = nullptr;
        switch (field.kind) {
        case ParamField::Int: {
            auto *row = new QWidget();
            auto *lay = new QHBoxLayout(row);
            lay->setContentsMargins(0, 0, 0, 0);
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setRange(int(field.min), int(field.max));
            auto *spin = new QSpinBox();
            spin->setRange(int(field.min), int(field.max));
            connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
            spin->setValue(field.def.toInt());
            lay->addWidget(slider, 1);
            lay->addWidget(spin);
            form->addRow(field.label, row);
            widget = spin;
            break;
        }
        case ParamField::Float: {
            auto *spin = new QDoubleSpinBox();
            spin->setRange(field.min, field.max);
            spin->setSingleStep(0.05);
            spin->setValue(field.def.toDouble());
            form->addRow(field.label, spin);
            widget = spin;
            break;
        }
        case ParamField::Choice: {
            auto *combo = new QComboBox();
            for (const auto &[value, text] : field.choices)
                combo->addItem(text, value);
            combo->setCurrentIndex(std::max(0, combo->findData(field.def)));
            form->addRow(field.label, combo);
            widget = combo;
            break;
        }
        case ParamField::Text: {
            auto *edit = new QLineEdit(field.def.toString());
            form->addRow(field.label, edit);
            widget = edit;
            break;
        }
        case ParamField::Color: {
            auto *btn = new ColorButton(field.def.value<QColor>());
            form->addRow(field.label, btn);
            widget = btn;
            break;
        }
        case ParamField::Bool: {
            auto *chk = new QCheckBox();
            chk->setChecked(field.def.toBool());
            form->addRow(field.label, chk);
            widget = chk;
            break;
        }
        }
        m_widgets.append({field.kind, {field.key, widget}});
    }
    form->addRow(okCancel(this));
}

QVariantMap ParamDialog::values() const
{
    QVariantMap out;
    for (const auto &[kind, entry] : m_widgets) {
        const QString &key = entry.first;
        QWidget *w = entry.second;
        switch (kind) {
        case ParamField::Int: out.insert(key, static_cast<QSpinBox *>(w)->value()); break;
        case ParamField::Float: out.insert(key, static_cast<QDoubleSpinBox *>(w)->value()); break;
        case ParamField::Choice: out.insert(key, static_cast<QComboBox *>(w)->currentData().toString()); break;
        case ParamField::Text: out.insert(key, static_cast<QLineEdit *>(w)->text()); break;
        case ParamField::Color: out.insert(key, static_cast<ColorButton *>(w)->color()); break;
        case ParamField::Bool: out.insert(key, static_cast<QCheckBox *>(w)->isChecked()); break;
        }
    }
    return out;
}

std::optional<QVariantMap> ask(const QString &title, const QList<ParamField> &fields, QWidget *parent)
{
    ParamDialog dlg(title, fields, parent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.values();
    return std::nullopt;
}

// ---------------------------------------------------------------- ResizeDialog

ResizeDialog::ResizeDialog(const QSize &size, const QString &title, QWidget *parent, bool withAnchor)
    : QDialog(parent)
    , m_orig(size)
{
    setWindowTitle(title);
    auto *form = new QFormLayout(this);
    m_mode = new QComboBox();
    m_mode->addItem(QStringLiteral("픽셀"), QStringLiteral("px"));
    m_mode->addItem(QStringLiteral("퍼센트"), QStringLiteral("pct"));
    form->addRow(QStringLiteral("단위"), m_mode);
    m_w = new QSpinBox();
    m_w->setRange(1, 50000);
    m_h = new QSpinBox();
    m_h->setRange(1, 50000);
    form->addRow(QStringLiteral("가로 (Width)"), m_w);
    form->addRow(QStringLiteral("세로 (Height)"), m_h);
    m_keep = new QCheckBox(QStringLiteral("가로 세로 비율 유지"));
    m_keep->setChecked(!withAnchor);
    form->addRow(QString(), m_keep);
    if (withAnchor) {
        m_anchor = new QComboBox();
        const QList<QPair<QString, QString>> anchors = {
            {QStringLiteral("center"), QStringLiteral("가운데")},
            {QStringLiteral("top-left"), QStringLiteral("왼쪽 위")},
            {QStringLiteral("top-right"), QStringLiteral("오른쪽 위")},
            {QStringLiteral("bottom-left"), QStringLiteral("왼쪽 아래")},
            {QStringLiteral("bottom-right"), QStringLiteral("오른쪽 아래")},
        };
        for (const auto &[value, text] : anchors)
            m_anchor->addItem(text, value);
        form->addRow(QStringLiteral("기준 위치"), m_anchor);
        m_fill = new ColorButton(QColor(Qt::white));
        form->addRow(QStringLiteral("배경색"), m_fill);
    }
    form->addRow(okCancel(this));
    setPx();
    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, &ResizeDialog::modeChanged);
    connect(m_w, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { linked(true); });
    connect(m_h, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { linked(false); });
}

void ResizeDialog::setPx()
{
    m_busy = true;
    m_w->setValue(m_orig.width());
    m_h->setValue(m_orig.height());
    m_busy = false;
}

void ResizeDialog::modeChanged()
{
    m_busy = true;
    if (m_mode->currentData().toString() == QLatin1String("pct")) {
        m_w->setValue(100);
        m_h->setValue(100);
    } else {
        m_w->setValue(m_orig.width());
        m_h->setValue(m_orig.height());
    }
    m_busy = false;
}

void ResizeDialog::linked(bool fromWidth)
{
    if (m_busy || !m_keep->isChecked())
        return;
    m_busy = true;
    if (m_mode->currentData().toString() == QLatin1String("pct")) {
        (fromWidth ? m_h : m_w)->setValue((fromWidth ? m_w : m_h)->value());
    } else if (fromWidth) {
        m_h->setValue(std::max(1, roundHalfEven(double(m_w->value()) * m_orig.height() / std::max(1, m_orig.width()))));
    } else {
        m_w->setValue(std::max(1, roundHalfEven(double(m_h->value()) * m_orig.width() / std::max(1, m_orig.height()))));
    }
    m_busy = false;
}

QSize ResizeDialog::resultSize() const
{
    if (m_mode->currentData().toString() == QLatin1String("pct")) {
        return QSize(std::max(1, roundHalfEven(m_orig.width() * m_w->value() / 100.0)),
                     std::max(1, roundHalfEven(m_orig.height() * m_h->value() / 100.0)));
    }
    return QSize(m_w->value(), m_h->value());
}

QString ResizeDialog::anchor() const
{
    return m_anchor ? m_anchor->currentData().toString() : QStringLiteral("center");
}

QColor ResizeDialog::fillColor() const
{
    return m_fill ? m_fill->color() : QColor(Qt::white);
}

} // namespace mm::editor
