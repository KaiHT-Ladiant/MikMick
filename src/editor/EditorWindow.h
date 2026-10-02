#pragma once

#include "editor/Canvas.h"

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QRect>
#include <QString>

#include <functional>

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QToolButton;

namespace mm {
class AppContext;
class Config;
} // namespace mm

namespace mm::editor {

class Backstage;
class ColorSwatch;
class PaletteGrid;
class Ribbon;
class RibbonButton;

class EditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    using EffectFn = std::function<QImage(const QImage &, const QRect &)>;

    explicit EditorWindow(AppContext &ctx, QWidget *parent = nullptr);

    // ---- API used by the application controller
    bool hasDocument() const;
    // page: "start", "new", "share", "thumbnail", "info"
    void showBackstage(const QString &page = QStringLiteral("start"));
    void hideBackstage();
    bool isBackstageVisible() const;
    void bringToFront();
    void centerOnScreen();
    // Opens a new document tab; title defaults to the file name or "이미지 N".
    Document *addImage(const QImage &image, const QString &path = QString(), const QString &title = QString());
    bool openPath(const QString &path);
    void applySettings();
    // Prompts for unsaved documents; false if the user cancelled.
    bool closeAll();

    // ---- documents
    int documentCount() const;
    Document *currentDocument() const;
    CanvasView *currentView() const;
    CanvasView *viewAt(int index) const;
    QList<CanvasView *> views() const;
    bool closeDocument(int index);
    void setCurrentIndex(int index);

    // ---- file
    void openFiles();
    bool save();
    bool saveAs();
    bool saveTo(Document *doc, const QString &path);
    void printImage();
    void share(const QString &target);

    // ---- edit
    void undo();
    void redo();
    void copy();
    void cut();
    void deleteSelection();
    void paste();
    void pasteAsNew();
    void selectAll();
    void deselect();
    void crop();
    void mergeObjects();
    // Applies fn to the selection (or whole image) as one undoable step.
    void applyEffect(const QString &name, const EffectFn &fn);
    // Clears the selection and applies fn to the whole image.
    void applyTransform(const QString &name, const Document::RasterFn &fn);

    // ---- tools
    ToolState &toolState() { return m_state; }
    const ToolState &toolState() const { return m_state; }
    void setTool(const QString &tool);
    void setDrawKind(const QString &kind);   // pen | highlighter | eraser
    void setShapeKind(const QString &kind);  // one of shapeKinds()
    void setStampKind(const QString &kind);  // number | cursor
    void setLineWidth(int width);
    void setColor(const QColor &color, bool first);
    void swapColors();
    void resetColors();
    void resetStampCounter();
    int nextStamp();

    // ---- zoom
    void setZoom(double zoom);
    void zoomBy(double factor);
    void fitToWindow();

    // ---- widgets (for tests)
    Ribbon *ribbon() const { return m_ribbon; }
    Backstage *backstage() const { return m_backstage; }
    QTabWidget *tabs() const { return m_tabs; }
    QSlider *zoomSlider() const { return m_zoomSlider; }
    QLabel *zoomLabel() const { return m_lblZoom; }
    RibbonButton *toolButton(const QString &tool) const;
    bool autoMergeOnSelect() const;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    struct MenuEntry
    {
        QString text;
        QString icon;
        std::function<void()> slot;
        bool separator = false;
    };
    static MenuEntry separator();

    QAction *act(const QString &text, const QString &iconName, std::function<void()> slot,
                 const QKeySequence &shortcut = QKeySequence());
    QMenu *menu(const QList<MenuEntry> &items);
    void buildActions();
    void buildRibbon();
    QMenu *shapeMenu();
    void buildStatus();
    QBrush bgBrush() const;
    void initialZoom(CanvasView *view);

    void fxBlur();
    void fxMosaic();
    void fxBrightness();
    void fxHue();
    void fxBorder();
    void fxShadow();
    void fxWatermark();
    void resizeImage();
    void resizeCanvas();

    void syncToolButtons();
    void stampSizeDialog();
    void fillToleranceDialog();
    void pickFont();
    void swatchClicked(bool first);
    void palettePicked(const QColor &color, bool primary);
    void moreColors();
    void setBgMode(const QString &mode);
    void toggleStatus(bool on);

    void onZoomChanged(double zoom);
    void refreshThumbnails();
    void onBackstage(const QString &kind, const QString &name);
    void onTabChanged(int index);
    void updateTitles();
    void updateUi();
    void showMessage(const QString &text, int ms);

    AppContext &m_ctx;
    Config &m_config;
    ToolState m_state;
    int m_stampNo = 0;
    bool m_statusVisible = true;

    QStackedWidget *m_stack = nullptr;
    Ribbon *m_ribbon = nullptr;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_empty = nullptr;
    QStackedWidget *m_canvasStack = nullptr;
    Backstage *m_backstage = nullptr;

    QAction *m_aNew = nullptr;
    QAction *m_aOpen = nullptr;
    QAction *m_aSave = nullptr;
    QAction *m_aSaveAs = nullptr;
    QAction *m_aPrint = nullptr;
    QAction *m_aUndo = nullptr;
    QAction *m_aRedo = nullptr;
    QAction *m_aPaste = nullptr;
    QAction *m_aPasteNew = nullptr;
    QAction *m_aCut = nullptr;
    QAction *m_aCopy = nullptr;
    QAction *m_aDelete = nullptr;
    QAction *m_aSelectAll = nullptr;
    QAction *m_aDeselect = nullptr;
    QAction *m_aCrop = nullptr;
    QAction *m_aZoomIn = nullptr;
    QAction *m_aZoomOut = nullptr;
    QAction *m_aZoom100 = nullptr;
    QAction *m_aZoomFit = nullptr;
    QAction *m_aMerge = nullptr;
    QAction *m_aClose = nullptr;
    QAction *m_aShapeFill = nullptr;
    QList<QAction *> m_docActions;
    QActionGroup *m_widthGroup = nullptr;

    QList<QPair<QString, RibbonButton *>> m_toolButtons;
    QList<QWidget *> m_docRibbonKeep;
    RibbonButton *m_btnWidth = nullptr;
    ColorSwatch *m_sw1 = nullptr;
    ColorSwatch *m_sw2 = nullptr;
    PaletteGrid *m_palette = nullptr;

    QLabel *m_lblSize = nullptr;
    QLabel *m_lblPos = nullptr;
    QLabel *m_lblSel = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_lblZoom = nullptr;
};

} // namespace mm::editor
