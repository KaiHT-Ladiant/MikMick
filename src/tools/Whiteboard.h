#pragma once

#include "capture/Backend.h"

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <optional>

class QLabel;

namespace mm::tools {

struct WhiteboardStroke
{
    QString kind; // pen | highlighter | line | arrow | rect | ellipse | eraser
    QColor color;
    int width = 4;
    QList<QPointF> points;
    QPointF end;

    void paint(QPainter &p) const;
};

// Presentation tool: draw directly over the desktop (transparent / screen / white / black background).
class Whiteboard : public QWidget
{
    Q_OBJECT
public:
    explicit Whiteboard(std::optional<capture::Snapshot> snapshot, QWidget *parent = nullptr);

    void start();
    void undo();
    void redo();
    void clear();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void buildToolbar();
    void setBackground(const QString &mode);
    void placeToolbar();

    std::optional<capture::Snapshot> m_snapshot;
    QString m_background;
    QString m_kind = QStringLiteral("pen");
    QColor m_color;
    int m_penWidth = 4;
    QList<WhiteboardStroke> m_strokes;
    QList<WhiteboardStroke> m_redo;
    std::optional<WhiteboardStroke> m_current;
    QPixmap m_layer;
    QWidget *m_bar = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace mm::tools
