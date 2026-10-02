#pragma once

#include "capture/FrozenOverlay.h"
#include "core/Config.h"

#include <QColor>
#include <QList>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QWidget>

class QColorDialog;
class QComboBox;
class QLineEdit;

namespace mm::tools {

// (key, label) pairs: html, hex, rgb, rgba, hsb, cpp, delphi.
const QList<QPair<QString, QString>> &colorFormats();
QString formatColor(const QColor &c, const QString &format);

// Frozen screenshot with magnifier; click / Enter / Space picks the colour under the cursor.
class ColorPickOverlay : public mm::capture::FrozenOverlay
{
    Q_OBJECT
public:
    ColorPickOverlay(const capture::Snapshot &snap, int zoom = 8);

Q_SIGNALS:
    void picked(const QColor &c);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void pick(const QPoint &pos);
};

class ColorSwatch;

// Picked colour + editable palette (custom colours persisted in config palette.custom).
class ColorToolWindow : public QWidget
{
    Q_OBJECT
public:
    ColorToolWindow(Config &config, bool paletteMode, QWidget *parent = nullptr);

    void setColor(const QColor &c);
    QColor color() const;
    QString codeText() const;
    void bringToFront();
    void bringToFront(const QPoint &pos);

    void copyCode();
    void saveCustom();

Q_SIGNALS:
    void pickAgain();

private:
    void formatChanged();
    void updateColor(const QColor &c);
    QString currentFormat() const;

    Config &m_config;
    QColorDialog *m_dialog = nullptr;
    ColorSwatch *m_swatch = nullptr;
    QComboBox *m_format = nullptr;
    QLineEdit *m_code = nullptr;
};

} // namespace mm::tools
