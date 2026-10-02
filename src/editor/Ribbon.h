#pragma once

#include <QColor>
#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

class QAction;
class QHBoxLayout;
class QMenu;
class QStackedWidget;
class QTabBar;

// Office-style ribbon widgets.
namespace mm::editor {

inline constexpr const char *kAccent = "#2b579a";

class RibbonButton : public QToolButton
{
    Q_OBJECT

public:
    RibbonButton(const QString &text, const QString &iconName, bool large = true, QMenu *menu = nullptr,
                 bool checkable = false);
    RibbonButton(const QString &text, const QIcon &icon, bool large = true, QMenu *menu = nullptr,
                 bool checkable = false);

private:
    void init(const QString &text, const QIcon &icon, bool large, QMenu *menu, bool checkable);
};

class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonGroup(const QString &title);

    QWidget *add(QWidget *widget);
    void addColumn(const QList<QWidget *> &widgets);

private:
    QHBoxLayout *m_content;
};

class RibbonPage : public QWidget
{
    Q_OBJECT

public:
    RibbonPage();

    RibbonGroup *addGroup(RibbonGroup *group);
    void finish();

private:
    QHBoxLayout *m_layout;
    bool m_stretchAdded = false;
};

// Colour 1 / colour 2 indicator button.
class ColorSwatch : public QToolButton
{
    Q_OBJECT

public:
    ColorSwatch(const QString &label, const QColor &color);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
    QString m_label;
};

class PaletteCell : public QToolButton
{
    Q_OBJECT

public:
    // An invalid colour is an empty custom slot.
    explicit PaletteCell(const QColor &color = QColor());

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

Q_SIGNALS:
    void picked(const QColor &color, bool primary);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
};

const QStringList &paletteColors();

class PaletteGrid : public QWidget
{
    Q_OBJECT

public:
    explicit PaletteGrid(const QStringList &custom);

    void setCustom(const QStringList &colors);
    QList<PaletteCell *> customCells() const { return m_customCells; }

Q_SIGNALS:
    void picked(const QColor &color, bool primary);

private:
    QList<PaletteCell *> m_customCells;
};

class Ribbon : public QWidget
{
    Q_OBJECT

public:
    Ribbon();

    QToolButton *addQuick(QAction *action);
    void addPage(const QString &title, RibbonPage *page);
    QHBoxLayout *extraLayout() const { return m_extra; }
    QTabBar *tabBar() const { return m_tabs; }
    QStackedWidget *stack() const { return m_stack; }
    bool isCollapsed() const { return m_collapsed; }
    void toggleCollapsed();
    // Enables/disables every page button except those in keep.
    void setBodyEnabled(bool enabled, const QList<QWidget *> &keep = {});

Q_SIGNALS:
    void fileClicked();

private:
    void onTab(int index);

    QHBoxLayout *m_quick;
    QHBoxLayout *m_extra;
    QTabBar *m_tabs;
    QWidget *m_body;
    QStackedWidget *m_stack;
    QToolButton *m_collapseBtn;
    int m_lastIndex = 1;
    bool m_collapsed = false;
};

QWidget *spacer();

} // namespace mm::editor
