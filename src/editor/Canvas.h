#pragma once

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsView>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointF>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QString>
#include <QUndoCommand>

#include <functional>
#include <tuple>

class QGraphicsItem;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QUndoStack;

namespace mm::editor {

class Document;

// Current tool and drawing options shared by every canvas of an editor window.
struct ToolState
{
    // move | select | draw | fill | text | stamp | shape | eyedropper
    QString tool = QStringLiteral("move");
    QColor color1 = QColor(QStringLiteral("#e81123"));
    QColor color2 = QColor(QStringLiteral("#ffffff"));
    int width = 3;
    QString drawKind = QStringLiteral("pen"); // pen | highlighter | eraser
    QString shapeKind = QStringLiteral("rect");
    bool shapeFill = false;
    QString stampKind = QStringLiteral("number"); // number | cursor
    int stampSize = 28;
    QFont font = QFont(QStringLiteral("Sans"), 16);
    int fillTolerance = 16;
};

// ---------------------------------------------------------------- undo commands
// Commands only hold raw item pointers; the Document owns every object it has seen
// (see Document::adopt) so items stay valid for the whole life of the undo stack.

class ImageCommand : public QUndoCommand
{
public:
    ImageCommand(Document *doc, const QImage &before, const QImage &after, const QString &text);
    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QImage m_before;
    QImage m_after;
};

class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(Document *doc, QGraphicsItem *item, const QString &text = QStringLiteral("개체 추가"),
                   bool alreadyAdded = false);
    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QGraphicsItem *m_item;
    bool m_skipFirst;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(Document *doc, const QList<QGraphicsItem *> &items,
                       const QString &text = QStringLiteral("개체 삭제"));
    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QList<QGraphicsItem *> m_items;
};

class MoveItemsCommand : public QUndoCommand
{
public:
    using Move = std::tuple<QGraphicsItem *, QPointF, QPointF>; // item, old pos, new pos
    explicit MoveItemsCommand(const QList<Move> &items);
    void redo() override;
    void undo() override;

private:
    QList<Move> m_items;
};

// ---------------------------------------------------------------- document

// One open image: raster background + vector objects in a QGraphicsScene, an optional
// rectangular selection and an undo stack.
class Document : public QObject
{
    Q_OBJECT

public:
    using RasterFn = std::function<QImage(const QImage &)>;

    explicit Document(const QImage &image, const QString &path = QString(), const QString &title = QString(),
                      QObject *parent = nullptr);
    ~Document() override;

    QString path() const { return m_path; }
    void setPath(const QString &path) { m_path = path; }
    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    QGraphicsScene *scene() const { return m_scene; }
    QUndoStack *undoStack() const { return m_undo; }
    QGraphicsPixmapItem *backgroundItem() const { return m_bgItem; }

    const QImage &image() const { return m_image; }
    QSize size() const { return m_image.size(); }
    // True for never-saved images (no path) or when the undo stack is not clean.
    bool isModified() const;

    // Top-level objects (excludes the background and selection marker), topmost first.
    QList<QGraphicsItem *> objects() const;

    bool hasSelection() const { return !m_selection.isNull(); }
    // Null rect when nothing is selected.
    QRect selection() const { return m_selection; }
    // Normalized and clipped to the image; rects smaller than 2x2 (or null) clear it.
    void setSelection(const QRect &rect);
    void clearSelection() { setSelection(QRect()); }

    // Background + objects, without selection decorations.
    QImage flatten();
    // Merges the objects into the image, then applies fn (one undo macro). False if fn
    // returned a null image.
    bool applyRaster(const RasterFn &fn, const QString &text);
    void mergeObjects();
    void addItem(QGraphicsItem *item, const QString &text = QStringLiteral("개체 추가"));
    void removeItems(const QList<QGraphicsItem *> &items);
    bool cropSelection();
    // Flattened image limited to the selection (whole image without one).
    QImage selectionImage();

    // Used by undo commands; not undoable by itself.
    void setBackground(const QImage &image);
    // Takes ownership of an object item (deleted with the document when not in the scene).
    void adopt(QGraphicsItem *item);

Q_SIGNALS:
    void changed();
    void selectionChanged();

private:
    QString m_path;
    QString m_title;
    QGraphicsScene *m_scene = nullptr;
    QUndoStack *m_undo = nullptr;
    QGraphicsPixmapItem *m_bgItem = nullptr;
    QGraphicsRectItem *m_selectionItem = nullptr;
    QImage m_image;
    QRect m_selection;
    QSet<QGraphicsItem *> m_owned;
};

// ---------------------------------------------------------------- view

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    // The view takes ownership of doc. state and the callbacks must outlive the view.
    CanvasView(Document *doc, ToolState *state, std::function<int()> stampCounter, const QBrush &bg, bool centered,
               QWidget *parent = nullptr);

    Document *document() const { return m_doc; }
    ToolState *toolState() const { return m_state; }
    double zoom() const { return m_zoom; }

    // When the provider returns true, starting a selection merges the objects first.
    void setAutoMergeProvider(std::function<bool()> provider) { m_autoMerge = std::move(provider); }

    void applyTool();
    void setZoom(double z);
    void fit();
    void pastePixmap(const QPixmap &pixmap);

    static QPointF constrain(const QPointF &origin, const QPointF &p, const QString &kind);

Q_SIGNALS:
    void cursorMoved(const QPoint &pos);
    void zoomChanged(double zoom);
    void colorPicked(const QColor &color, bool primary);
    void toolFinished();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPointF scenePoint(QMouseEvent *event) const;
    QPointF clamped(const QPointF &p) const;

    Document *m_doc;
    ToolState *m_state;
    std::function<int()> m_nextStamp;
    std::function<bool()> m_autoMerge;
    double m_zoom = 1.0;
    QGraphicsItem *m_temp = nullptr;
    bool m_hasOrigin = false;
    QPointF m_origin;
    QList<QPair<QGraphicsItem *, QPointF>> m_moveStart;
    bool m_panning = false;
    QPoint m_panPos;
};

} // namespace mm::editor
