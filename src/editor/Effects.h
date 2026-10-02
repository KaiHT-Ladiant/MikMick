#pragma once

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QRect>
#include <QString>

// Image effects and filters (QImage -> QImage). Pixel filters work on non-premultiplied
// Format_ARGB32 data and only touch `rect` (a null rect means the whole image).
namespace mm::editor::effects {

QImage blur(const QImage &img, int radius = 4, const QRect &rect = QRect());
QImage sharpen(const QImage &img, double amount = 1.0, const QRect &rect = QRect());
QImage mosaic(const QImage &img, int block = 10, const QRect &rect = QRect());
QImage grayscale(const QImage &img, const QRect &rect = QRect());
QImage invert(const QImage &img, const QRect &rect = QRect());
// brightness, contrast: -100 ~ 100
QImage brightnessContrast(const QImage &img, int brightness = 0, int contrast = 0, const QRect &rect = QRect());
// hue: -180 ~ 180, saturation / lightness: -100 ~ 100
QImage hueSaturation(const QImage &img, int hue = 0, int saturation = 0, int lightness = 0,
                     const QRect &rect = QRect());

QImage rotate(const QImage &img, int degrees);
QImage flip(const QImage &img, bool horizontal);
QImage resize(const QImage &img, int width, int height, bool smooth = true);
// anchor: "center", "left", "right", or "<top|center|bottom>-<left|center|right>"
QImage canvasSize(const QImage &img, int width, int height, const QString &anchor = QStringLiteral("center"),
                  const QColor &fill = QColor(Qt::white));
QImage border(const QImage &img, int width = 4, const QColor &color = QColor(Qt::black), bool inside = false);
QImage dropShadow(const QImage &img, int offset = 8, int radius = 6, const QColor &bg = QColor(0, 0, 0, 0));
// position: "top-left", "top-right", "bottom-left", "bottom-right", "center", "tile"
QImage watermark(const QImage &img, const QString &text, const QColor &color = QColor(Qt::white),
                 double opacity = 0.35, const QString &position = QStringLiteral("bottom-right"), int size = 24);
QImage fillRect(const QImage &img, const QRect &rect, const QColor &color);
QImage cropToPath(const QImage &img, const QPainterPath &path);
// Scanline flood fill; a pixel matches when every B/G/R/A channel differs by <= tolerance.
QImage floodFill(const QImage &img, int x, int y, const QColor &color, int tolerance = 16);

} // namespace mm::editor::effects
