#include "editor/Canvas.h"

#include "editor/Effects.h"
#include "editor/Items.h"

#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScrollBar>
#include <QUndoStack>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace mm::editor {

// ---------------------------------------------------------------- undo commands

ImageCommand::ImageCommand(Document *doc, const QImage &before, const QImage &after, const QString &text)
    : QUndoCommand(text)
    , m_doc(doc)
    , m_before(before)
    , m_after(after)
{
}

void ImageCommand::redo()
{
    m_doc->setBackground(m_after);
}

void ImageCommand::undo()
{
    m_doc->setBackground(m_before);
}

AddItemCommand::AddItemCommand(Document *doc, QGraphicsItem *item, const QString &text, bool alreadyAdded)
    : QUndoCommand(text)
    , m_doc(doc)
    , m_item(item)
    , m_skipFirst(alreadyAdded)
{
    doc->adopt(item);
}

void AddItemCommand::redo()
{
    if (m_skipFirst) {
        m_skipFirst = false;
        return;
    }
    if (!m_item->scene())
        m_doc->scene()->addItem(m_item);
}

void AddItemCommand::undo()
{
    if (m_item->scene())
        m_doc->scene()->removeItem(m_item);
}

RemoveItemsCommand::RemoveItemsCommand(Document *doc, const QList<QGraphicsItem *> &items, const QString &text)
    : QUndoCommand(text)
    , m_doc(doc)
    , m_items(items)
{
    for (QGraphicsItem *it : items)
        doc->adopt(it);
}

void RemoveItemsCommand::redo()
{
    for (QGraphicsItem *it : std::as_const(m_items)) {
        if (it->scene())
            m_doc->scene()->removeItem(it);
    }
}

void RemoveItemsCommand::undo()
{
    for (QGraphicsItem *it : std::as_const(m_items)) {
        if (!it->scene())
            m_doc->scene()->addItem(it);
    }
}

MoveItemsCommand::MoveItemsCommand(const QList<Move> &items)
    : QUndoCommand(QStringLiteral("개체 이동"))
    , m_items(items)
{
}

void MoveItemsCommand::redo()
{
    for (const auto &[item, oldPos, newPos] : std::as_const(m_items)) {
        Q_UNUSED(oldPos)
        item->setPos(newPos);
    }
}

void MoveItemsCommand::undo()
{
    for (const auto &[item, oldPos, newPos] : std::as_const(m_items)) {
        Q_UNUSED(newPos)
        item->setPos(oldPos);
    }
}

// ---------------------------------------------------------------- document

Document::Document(const QImage &image, const QString &path, const QString &title, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
    m_title = !title.isEmpty() ? title : (!path.isEmpty() ? QFileInfo(path).fileName() : QStringLiteral("제목 없음"));
    m_scene = new QGraphicsScene(this);
    m_undo = new QUndoStack(this);
    m_bgItem = new QGraphicsPixmapItem();
    m_bgItem->setZValue(-1000);
    m_bgItem->setTransformationMode(Qt::FastTransformation);
    m_scene->addItem(m_bgItem);
    m_selectionItem = new QGraphicsRectItem();
    QPen pen(QColor(0, 0, 0), 0, Qt::DashLine);
    pen.setCosmetic(true);
    m_selectionItem->setPen(pen);
    m_selectionItem->setBrush(QBrush(QColor(47, 140, 255, 30)));
    m_selectionItem->setZValue(10000);
    m_selectionItem->hide();
    m_scene->addItem(m_selectionItem);
    setBackground(image.convertToFormat(QImage::Format_ARGB32));
    connect(m_undo, &QUndoStack::indexChanged, this, [this](int) { Q_EMIT changed(); });
}

Document::~Document()
{
    blockSignals(true);
    m_undo->blockSignals(true);
    if (m_scene->focusItem())
        m_scene->setFocusItem(nullptr);
    m_undo->clear();
    for (QGraphicsItem *item : std::as_const(m_owned)) {
        if (item->scene() != m_scene)
            delete item;
    }
    m_owned.clear();
    delete m_scene;
    m_scene = nullptr;
}

bool Document::isModified() const
{
    return m_path.isEmpty() || !m_undo->isClean();
}

QList<QGraphicsItem *> Document::objects() const
{
    QList<QGraphicsItem *> out;
    const QList<QGraphicsItem *> all = m_scene->items();
    for (QGraphicsItem *it : all) {
        if (it != m_bgItem && it != m_selectionItem && !it->parentItem())
            out << it;
    }
    return out;
}

void Document::setBackground(const QImage &image)
{
    m_image = image;
    m_bgItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(QRectF(image.rect()));
    if (hasSelection() && !image.rect().contains(m_selection))
        setSelection(QRect());
    Q_EMIT changed();
}

void Document::setSelection(const QRect &rect)
{
    QRect r;
    if (!rect.isNull()) {
        r = rect.normalized().intersected(m_image.rect());
        if (r.width() < 2 || r.height() < 2)
            r = QRect();
    }
    m_selection = r;
    if (r.isNull()) {
        m_selectionItem->hide();
    } else {
        m_selectionItem->setRect(QRectF(r));
        m_selectionItem->show();
    }
    Q_EMIT selectionChanged();
}

QImage Document::flatten()
{
    QImage out(m_image.size(), QImage::Format_ARGB32);
    out.fill(0);
    const QList<QGraphicsItem *> selected = m_scene->selectedItems();
    for (QGraphicsItem *it : selected)
        it->setSelected(false);
    const bool selVisible = m_selectionItem->isVisible();
    m_selectionItem->hide();
    if (QGraphicsItem *focus = m_scene->focusItem())
        focus->clearFocus();
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    m_scene->render(&painter, QRectF(out.rect()), QRectF(out.rect()));
    painter.end();
    m_selectionItem->setVisible(selVisible);
    for (QGraphicsItem *it : selected) {
        if (it->scene() == m_scene)
            it->setSelected(true);
    }
    return out;
}

bool Document::applyRaster(const RasterFn &fn, const QString &text)
{
    const QList<QGraphicsItem *> objs = objects();
    const QImage before = m_image;
    const QImage base = objs.isEmpty() ? m_image : flatten();
    const QImage after = fn(base);
    if (after.isNull())
        return false;
    m_undo->beginMacro(text);
    if (!objs.isEmpty())
        m_undo->push(new RemoveItemsCommand(this, objs, QStringLiteral("개체 병합")));
    m_undo->push(new ImageCommand(this, before, after, text));
    m_undo->endMacro();
    return true;
}

void Document::mergeObjects()
{
    if (!objects().isEmpty())
        applyRaster([](const QImage &img) { return img; }, QStringLiteral("개체 병합"));
}

void Document::addItem(QGraphicsItem *item, const QString &text)
{
    m_undo->push(new AddItemCommand(this, item, text));
}

void Document::removeItems(const QList<QGraphicsItem *> &items)
{
    if (!items.isEmpty())
        m_undo->push(new RemoveItemsCommand(this, items));
}

bool Document::cropSelection()
{
    if (!hasSelection())
        return false;
    const QRect rect = m_selection;
    clearSelection();
    applyRaster([rect](const QImage &img) { return img.copy(rect); }, QStringLiteral("자르기"));
    return true;
}

QImage Document::selectionImage()
{
    const QImage flat = flatten();
    if (hasSelection())
        return flat.copy(m_selection);
    return flat;
}

void Document::adopt(QGraphicsItem *item)
{
    if (item)
        m_owned.insert(item);
}

// ---------------------------------------------------------------- view

CanvasView::CanvasView(Document *doc, ToolState *state, std::function<int()> stampCounter, const QBrush &bg,
                       bool centered, QWidget *parent)
    : QGraphicsView(doc->scene(), parent)
    , m_doc(doc)
    , m_state(state)
    , m_nextStamp(std::move(stampCounter))
{
    doc->setParent(this);
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(bg);
    setAlignment(centered ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignTop));
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    applyTool();
}

void CanvasView::applyTool()
{
    const QString tool = m_state->tool;
    const bool interactive = tool == QLatin1String("move");
    setDragMode(interactive ? QGraphicsView::RubberBandDrag : QGraphicsView::NoDrag);
    const QList<QGraphicsItem *> objs = m_doc->objects();
    for (QGraphicsItem *it : objs) {
        it->setFlag(QGraphicsItem::ItemIsMovable, interactive);
        it->setFlag(QGraphicsItem::ItemIsSelectable, interactive || tool == QLatin1String("text"));
    }
    if (!interactive)
        m_doc->scene()->clearSelection();
    Qt::CursorShape cursor = Qt::ArrowCursor;
    if (tool == QLatin1String("select") || tool == QLatin1String("draw") || tool == QLatin1String("stamp")
        || tool == QLatin1String("shape") || tool == QLatin1String("eyedropper"))
        cursor = Qt::CrossCursor;
    else if (tool == QLatin1String("fill"))
        cursor = Qt::PointingHandCursor;
    else if (tool == QLatin1String("text"))
        cursor = Qt::IBeamCursor;
    viewport()->setCursor(cursor);
}

void CanvasView::setZoom(double z)
{
    z = std::clamp(z, 0.05, 32.0);
    m_zoom = z;
    resetTransform();
    scale(z, z);
    Q_EMIT zoomChanged(z);
}

void CanvasView::fit()
{
    const QRectF r = m_doc->scene()->sceneRect();
    const QSize vp = viewport()->size();
    if (r.isEmpty() || vp.isEmpty())
        return;
    const double z = std::min((vp.width() - 20) / r.width(), (vp.height() - 20) / r.height());
    setZoom(std::min(1.0, z));
}

QPointF CanvasView::scenePoint(QMouseEvent *event) const
{
    return mapToScene(event->position().toPoint());
}

QPointF CanvasView::clamped(const QPointF &p) const
{
    const QRectF r = m_doc->scene()->sceneRect();
    return QPointF(std::min(std::max(p.x(), r.left()), r.right()), std::min(std::max(p.y(), r.top()), r.bottom()));
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panPos = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    const QString tool = m_state->tool;
    const QPointF p = scenePoint(event);
    QGraphicsScene *scene = m_doc->scene();
    if (tool == QLatin1String("move")) {
        QGraphicsView::mousePressEvent(event);
        m_moveStart.clear();
        const QList<QGraphicsItem *> selected = scene->selectedItems();
        for (QGraphicsItem *it : selected)
            m_moveStart.append({it, it->pos()});
        return;
    }
    if (tool == QLatin1String("text")) {
        if (dynamic_cast<TextItem *>(scene->itemAt(p, transform()))) {
            QGraphicsView::mousePressEvent(event);
            return;
        }
        if (auto *focus = dynamic_cast<TextItem *>(scene->focusItem())) {
            focus->clearFocus();
            return;
        }
    }
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;
    const bool primary = event->button() == Qt::LeftButton;
    const QColor color = primary ? m_state->color1 : m_state->color2;
    if (tool == QLatin1String("eyedropper")) {
        const QImage img = m_doc->flatten();
        const QPoint ip = p.toPoint();
        if (img.rect().contains(ip))
            Q_EMIT colorPicked(img.pixelColor(ip), primary);
        return;
    }
    if (tool == QLatin1String("fill")) {
        const QPoint ip = p.toPoint();
        if (m_doc->image().rect().contains(ip)) {
            const int tol = m_state->fillTolerance;
            m_doc->applyRaster(
                [ip, color, tol](const QImage &img) { return effects::floodFill(img, ip.x(), ip.y(), color, tol); },
                QStringLiteral("채우기"));
        }
        return;
    }
    if (tool == QLatin1String("text")) {
        auto *item = new TextItem(QStringLiteral("텍스트"), m_state->font, color);
        item->setPos(p);
        m_doc->addItem(item, QStringLiteral("텍스트"));
        item->startEditing();
        return;
    }
    if (tool == QLatin1String("stamp")) {
        StampItem *item = nullptr;
        if (m_state->stampKind == QLatin1String("cursor"))
            item = new StampItem(QStringLiteral("cursor"), 0, color, m_state->stampSize);
        else
            item = new StampItem(QStringLiteral("number"), m_nextStamp ? m_nextStamp() : 1, color, m_state->stampSize);
        item->setPos(p);
        m_doc->addItem(item, QStringLiteral("스탬프"));
        return;
    }
    m_origin = p;
    m_hasOrigin = true;
    if (tool == QLatin1String("select")) {
        if (!m_doc->objects().isEmpty() && m_autoMerge && m_autoMerge())
            m_doc->mergeObjects();
        m_doc->clearSelection();
    } else if (tool == QLatin1String("draw")) {
        const QString kind = m_state->drawKind;
        StrokeItem *item = nullptr;
        if (kind == QLatin1String("highlighter")) {
            QColor c(color);
            c.setAlpha(255);
            item = new StrokeItem(p, c, std::max(8, m_state->width * 4), true);
        } else if (kind == QLatin1String("eraser")) {
            item = new StrokeItem(p, m_state->color2, std::max(6, m_state->width * 3));
        } else {
            item = new StrokeItem(p, color, m_state->width);
        }
        scene->addItem(item);
        m_temp = item;
    } else if (tool == QLatin1String("shape")) {
        QColor fill = m_state->shapeFill ? m_state->color2 : QColor();
        if (!primary && fill.isValid())
            fill = m_state->color1;
        auto *item = new ShapeItem(m_state->shapeKind, p, p, color, m_state->width, fill);
        scene->addItem(item);
        m_temp = item;
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint pos = event->position().toPoint();
        const QPoint delta = pos - m_panPos;
        m_panPos = pos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }
    const QPointF p = scenePoint(event);
    Q_EMIT cursorMoved(p.toPoint());
    const QString tool = m_state->tool;
    if (tool == QLatin1String("move") || tool == QLatin1String("text")) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (!m_hasOrigin)
        return;
    if (tool == QLatin1String("select")) {
        m_doc->setSelection(QRectF(m_origin, clamped(p)).toAlignedRect());
    } else if (auto *stroke = qgraphicsitem_cast<StrokeItem *>(m_temp)) {
        stroke->addPoint(p);
    } else if (auto *shape = qgraphicsitem_cast<ShapeItem *>(m_temp)) {
        QPointF end = p;
        if (event->modifiers() & Qt::ShiftModifier)
            end = constrain(m_origin, p, shape->kind());
        shape->setEnd(end);
    }
}

QPointF CanvasView::constrain(const QPointF &o, const QPointF &p, const QString &kind)
{
    const double dx = p.x() - o.x();
    const double dy = p.y() - o.y();
    if (kind == QLatin1String("line") || kind == QLatin1String("arrow")) {
        if (std::abs(dx) > 2 * std::abs(dy))
            return QPointF(p.x(), o.y());
        if (std::abs(dy) > 2 * std::abs(dx))
            return QPointF(o.x(), p.y());
    }
    const double s = std::max(std::abs(dx), std::abs(dy));
    return QPointF(o.x() + (dx >= 0 ? s : -s), o.y() + (dy >= 0 ? s : -s));
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && event->button() == Qt::MiddleButton) {
        m_panning = false;
        applyTool();
        return;
    }
    const QString tool = m_state->tool;
    if (tool == QLatin1String("move") || tool == QLatin1String("text")) {
        QGraphicsView::mouseReleaseEvent(event);
        QList<MoveItemsCommand::Move> moved;
        for (const auto &[item, oldPos] : std::as_const(m_moveStart)) {
            if (item->pos() != oldPos)
                moved.append({item, oldPos, item->pos()});
        }
        m_moveStart.clear();
        if (!moved.isEmpty())
            m_doc->undoStack()->push(new MoveItemsCommand(moved));
        return;
    }
    if (m_temp) {
        QGraphicsItem *item = m_temp;
        m_temp = nullptr;
        const QRectF r = item->boundingRect();
        auto *shape = qgraphicsitem_cast<ShapeItem *>(item);
        bool discard = false;
        if (shape) {
            const QRectF sr = QRectF(shape->p1(), shape->p2()).normalized();
            discard = sr.width() < 2 && sr.height() < 2;
        }
        if (discard || r.isEmpty()) {
            m_doc->scene()->removeItem(item);
            delete item;
        } else {
            const QString name = qgraphicsitem_cast<StrokeItem *>(item) ? QStringLiteral("그리기") : QStringLiteral("도형");
            m_doc->undoStack()->push(new AddItemCommand(m_doc, item, name, true));
        }
    }
    m_hasOrigin = false;
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        setZoom(m_zoom * factor);
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    QGraphicsScene *scene = m_doc->scene();
    auto *focus = dynamic_cast<TextItem *>(scene->focusItem());
    if (focus && focus->textInteractionFlags() != Qt::NoTextInteraction) {
        if (event->key() == Qt::Key_Escape) {
            focus->clearFocus();
            return;
        }
        QGraphicsView::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        m_doc->clearSelection();
        scene->clearSelection();
        return;
    }
    const QList<QGraphicsItem *> selected = scene->selectedItems();
    const int key = event->key();
    if (!selected.isEmpty() && (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down)) {
        const double step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
        QPointF d;
        if (key == Qt::Key_Left)
            d = QPointF(-step, 0);
        else if (key == Qt::Key_Right)
            d = QPointF(step, 0);
        else if (key == Qt::Key_Up)
            d = QPointF(0, -step);
        else
            d = QPointF(0, step);
        QList<MoveItemsCommand::Move> moves;
        for (QGraphicsItem *it : selected)
            moves.append({it, it->pos(), it->pos() + d});
        m_doc->undoStack()->push(new MoveItemsCommand(moves));
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::pastePixmap(const QPixmap &pixmap)
{
    auto *item = new PixmapObject(pixmap);
    const QPointF tl = mapToScene(QPoint(10, 10));
    item->setPos(clamped(tl));
    m_doc->addItem(item, QStringLiteral("붙여넣기"));
    m_state->tool = QStringLiteral("move");
    applyTool();
    m_doc->scene()->clearSelection();
    item->setSelected(true);
    Q_EMIT toolFinished();
}

} // namespace mm::editor
