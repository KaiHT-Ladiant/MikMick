#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QPair>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;

// "File" tab backstage screens (start / new / share / thumbnail / info).
namespace mm::editor {

// Icon + title + description entry of the start page.
class TaskItem : public QToolButton
{
    Q_OBJECT

public:
    TaskItem(const QString &iconName, const QString &title, const QString &desc);
};

class BigButton : public QToolButton
{
    Q_OBJECT

public:
    BigButton(const QString &iconName, const QString &text);
};

struct CanvasPreset
{
    QString key;
    QString label;
    QSize size; // invalid for clipboard / screen / custom
};
const QList<CanvasPreset> &canvasPresets();

class StartPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(bool hideStart);

    QCheckBox *hideCheck() const { return m_hideCheck; }

Q_SIGNALS:
    void action(const QString &kind, const QString &name); // kind: new | open | capture | tool
    void hideChanged(bool hide);

private:
    QCheckBox *m_hideCheck;
};

class NewPage : public QWidget
{
    Q_OBJECT

public:
    NewPage(const QString &preset, const QSize &size, const QColor &bg);

    QString presetKey() const;
    QSize canvasSize() const;
    QColor background() const { return m_bg; }

Q_SIGNALS:
    void createRequested(const QSize &size, const QColor &bg);

private:
    void applyPreset();
    void manual();
    void swap();
    void updateLabel();
    void pickBackground();
    void updateBackground();

    QColor m_bg;
    QLabel *m_sizeLabel;
    QComboBox *m_preset;
    QSpinBox *m_w;
    QSpinBox *m_h;
    QToolButton *m_bgBtn;
};

class SharePage : public QWidget
{
    Q_OBJECT

public:
    SharePage();

Q_SIGNALS:
    void share(const QString &target);
};

class ThumbnailPage : public QWidget
{
    Q_OBJECT

public:
    ThumbnailPage();

    void setDocuments(const QList<QPair<QString, QPixmap>> &docs);

Q_SIGNALS:
    void activate(int index);

private:
    QScrollArea *m_area;
};

class InfoPage : public QWidget
{
    Q_OBJECT

public:
    InfoPage();

Q_SIGNALS:
    void homepage();
    void checkUpdate();
};

class Backstage : public QWidget
{
    Q_OBJECT

public:
    Backstage(bool hideStart, const QString &preset, const QSize &size, const QColor &bg);

    // Pages: start, new, share, thumbnail, info.
    void showPage(const QString &key);
    QString currentPage() const;
    void setHasDocument(bool hasDoc);

    StartPage *startPage() const { return m_startPage; }
    NewPage *newPage() const { return m_newPage; }
    SharePage *sharePage() const { return m_sharePage; }
    ThumbnailPage *thumbnailPage() const { return m_thumbPage; }
    InfoPage *infoPage() const { return m_infoPage; }
    QPushButton *sideButton(const QString &key) const { return m_buttons.value(key); }

    static const QStringList &pageItems();

Q_SIGNALS:
    void back();
    // kind: create (name "WxH:#aarrggbb"), open, save, save_as, print, close, share (target),
    // thumbnails, activate (index), options, capture (mode), tool (id), homepage, check_update
    void command(const QString &kind, const QString &name);

private:
    void onStartAction(const QString &kind, const QString &name);
    void onSide(const QString &key);

    QHash<QString, QPushButton *> m_buttons;
    QHash<QString, QWidget *> m_pages;
    QStackedWidget *m_stack;
    StartPage *m_startPage;
    NewPage *m_newPage;
    SharePage *m_sharePage;
    ThumbnailPage *m_thumbPage;
    InfoPage *m_infoPage;
};

} // namespace mm::editor
