#pragma once

#include <QColor>
#include <QDialog>
#include <QHash>
#include <QList>
#include <QPair>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <optional>

class QCheckBox;
class QComboBox;
class QSpinBox;

// Effect parameter and resize dialogs.
namespace mm::editor {

class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(const QColor &color, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

private:
    void pick();
    void refresh();

    QColor m_color;
};

struct ParamField
{
    enum Kind { Int, Float, Choice, Text, Color, Bool };

    QString key;
    QString label;
    Kind kind = Int;
    QVariant def;
    double min = 0;
    double max = 0;
    QList<QPair<QString, QString>> choices; // (value, label)

    static ParamField integer(const QString &key, const QString &label, int def, int min, int max);
    static ParamField real(const QString &key, const QString &label, double def, double min, double max);
    static ParamField choice(const QString &key, const QString &label, const QString &def,
                             const QList<QPair<QString, QString>> &choices);
    static ParamField text(const QString &key, const QString &label, const QString &def);
    static ParamField color(const QString &key, const QString &label, const QColor &def);
    static ParamField boolean(const QString &key, const QString &label, bool def);
};

// Builds a simple form from field definitions. values(): int -> int, float -> double,
// choice/text -> QString, color -> QColor, bool -> bool.
class ParamDialog : public QDialog
{
    Q_OBJECT

public:
    ParamDialog(const QString &title, const QList<ParamField> &fields, QWidget *parent = nullptr);

    QVariantMap values() const;

private:
    QList<QPair<ParamField::Kind, QPair<QString, QWidget *>>> m_widgets;
};

std::optional<QVariantMap> ask(const QString &title, const QList<ParamField> &fields, QWidget *parent = nullptr);

class ResizeDialog : public QDialog
{
    Q_OBJECT

public:
    ResizeDialog(const QSize &size, const QString &title = QStringLiteral("이미지 크기 조절"),
                 QWidget *parent = nullptr, bool withAnchor = false);

    QSize resultSize() const;
    // Canvas-size mode only ("center" otherwise).
    QString anchor() const;
    QColor fillColor() const;

    QComboBox *modeCombo() const { return m_mode; }
    QSpinBox *widthSpin() const { return m_w; }
    QSpinBox *heightSpin() const { return m_h; }
    QCheckBox *keepRatio() const { return m_keep; }

private:
    void setPx();
    void modeChanged();
    void linked(bool fromWidth);

    QSize m_orig;
    QComboBox *m_mode;
    QSpinBox *m_w;
    QSpinBox *m_h;
    QCheckBox *m_keep;
    QComboBox *m_anchor = nullptr;
    ColorButton *m_fill = nullptr;
    bool m_busy = false;
};

} // namespace mm::editor
