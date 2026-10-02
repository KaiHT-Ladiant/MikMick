#include "editor/Effects.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace mm::editor::effects {

namespace {

// Channel order follows the in-memory layout of Format_ARGB32: B, G, R, A.
struct Pixels
{
    int w = 0;
    int h = 0;
    std::vector<double> v;

    Pixels(int width, int height)
        : w(width)
        , h(height)
        , v(size_t(width) * size_t(height) * 4, 0.0)
    {
    }
    double *px(int x, int y) { return v.data() + (size_t(y) * size_t(w) + size_t(x)) * 4; }
    const double *px(int x, int y) const { return v.data() + (size_t(y) * size_t(w) + size_t(x)) * 4; }
};

QImage argb(const QImage &img)
{
    return img.convertToFormat(QImage::Format_ARGB32);
}

Pixels toPixels(const QImage &src)
{
    const QImage img = argb(src);
    Pixels p(img.width(), img.height());
    for (int y = 0; y < p.h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        double *d = p.px(0, y);
        for (int x = 0; x < p.w; ++x, d += 4) {
            const QRgb c = line[x];
            d[0] = qBlue(c);
            d[1] = qGreen(c);
            d[2] = qRed(c);
            d[3] = qAlpha(c);
        }
    }
    return p;
}

// Same as numpy clip(0, 255) followed by astype(uint8): truncates.
inline int toByte(double v)
{
    if (!(v > 0.0))
        return 0;
    if (v >= 255.0)
        return 255;
    return int(v);
}

QImage fromPixels(const Pixels &p)
{
    QImage out(p.w, p.h, QImage::Format_ARGB32);
    for (int y = 0; y < p.h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        const double *d = p.px(0, y);
        for (int x = 0; x < p.w; ++x, d += 4)
            line[x] = qRgba(toByte(d[2]), toByte(d[1]), toByte(d[0]), toByte(d[3]));
    }
    return out;
}

template<typename Fn>
QImage applyRegion(const QImage &img, const QRect &rect, Fn fn)
{
    if (rect.isNull())
        return fn(img);
    const QRect r = rect.intersected(img.rect());
    if (r.isEmpty())
        return img;
    const QImage part = fn(img.copy(r));
    QImage out = argb(img);
    QPainter painter(&out);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(r.topLeft(), part);
    painter.end();
    return out;
}

// Centered box filter of size 2r+1 with edge clamping (axis 0 = vertical, 1 = horizontal).
// Uses the same edge-padded prefix sums as the reference implementation so the rounding
// (and therefore the truncated 8-bit result) is identical.
void boxBlurAxis(Pixels &p, int r, int axis)
{
    if (r <= 0)
        return;
    const int n = axis == 0 ? p.h : p.w;
    const int lines = axis == 0 ? p.w : p.h;
    if (n <= 0)
        return;
    const size_t stride = axis == 0 ? size_t(p.w) * 4 : 4;
    const double div = 2.0 * r + 1.0;
    const int padded = n + 2 * r + 1;
    std::vector<double> src(static_cast<size_t>(n));
    std::vector<double> cum(static_cast<size_t>(padded));
    for (int line = 0; line < lines; ++line) {
        for (int c = 0; c < 4; ++c) {
            const size_t base = axis == 0 ? size_t(line) * 4 + size_t(c) : size_t(line) * size_t(p.w) * 4 + size_t(c);
            for (int i = 0; i < n; ++i)
                src[size_t(i)] = p.v[base + size_t(i) * stride];
            double s = 0.0;
            for (int j = 0; j < padded; ++j) {
                s += src[size_t(std::clamp(j - (r + 1), 0, n - 1))];
                cum[size_t(j)] = s;
            }
            for (int i = 0; i < n; ++i)
                p.v[base + size_t(i) * stride] = (cum[size_t(i + 2 * r + 1)] - cum[size_t(i)]) / div;
        }
    }
}

double floorMod(double a, double m)
{
    double r = std::fmod(a, m);
    if (r != 0.0 && ((r < 0.0) != (m < 0.0)))
        r += m;
    return r;
}

int floorDiv2(int v)
{
    return v >= 0 ? v / 2 : -((-v + 1) / 2);
}

} // namespace

QImage blur(const QImage &img, int radius, const QRect &rect)
{
    return applyRegion(img, rect, [radius](const QImage &im) {
        Pixels a = toPixels(im);
        for (int i = 0; i < 3; ++i) {
            boxBlurAxis(a, radius, 0);
            boxBlurAxis(a, radius, 1);
        }
        return fromPixels(a);
    });
}

QImage sharpen(const QImage &img, double amount, const QRect &rect)
{
    return applyRegion(img, rect, [amount](const QImage &im) {
        Pixels a = toPixels(im);
        Pixels b = a;
        boxBlurAxis(b, 1, 0);
        boxBlurAxis(b, 1, 1);
        for (size_t i = 0; i < a.v.size(); i += 4) {
            for (size_t c = 0; c < 3; ++c)
                a.v[i + c] = a.v[i + c] + amount * (a.v[i + c] - b.v[i + c]);
        }
        return fromPixels(a);
    });
}

QImage mosaic(const QImage &img, int block, const QRect &rect)
{
    return applyRegion(img, rect, [block](const QImage &im) {
        QImage out = argb(im);
        if (block <= 0)
            return out;
        const int w = out.width();
        const int h = out.height();
        for (int y0 = 0; y0 < h; y0 += block) {
            const int y1 = std::min(h, y0 + block);
            for (int x0 = 0; x0 < w; x0 += block) {
                const int x1 = std::min(w, x0 + block);
                double sum[4] = {0, 0, 0, 0};
                for (int y = y0; y < y1; ++y) {
                    const QRgb *line = reinterpret_cast<const QRgb *>(out.constScanLine(y));
                    for (int x = x0; x < x1; ++x) {
                        sum[0] += qBlue(line[x]);
                        sum[1] += qGreen(line[x]);
                        sum[2] += qRed(line[x]);
                        sum[3] += qAlpha(line[x]);
                    }
                }
                const double count = double(y1 - y0) * double(x1 - x0);
                const QRgb mean = qRgba(int(sum[2] / count), int(sum[1] / count), int(sum[0] / count),
                                        int(sum[3] / count));
                for (int y = y0; y < y1; ++y) {
                    QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
                    std::fill(line + x0, line + x1, mean);
                }
            }
        }
        return out;
    });
}

QImage grayscale(const QImage &img, const QRect &rect)
{
    return applyRegion(img, rect, [](const QImage &im) {
        Pixels a = toPixels(im);
        for (size_t i = 0; i < a.v.size(); i += 4) {
            const double g = 0.114 * a.v[i] + 0.587 * a.v[i + 1] + 0.299 * a.v[i + 2];
            a.v[i] = a.v[i + 1] = a.v[i + 2] = g;
        }
        return fromPixels(a);
    });
}

QImage invert(const QImage &img, const QRect &rect)
{
    return applyRegion(img, rect, [](const QImage &im) {
        QImage out = argb(im);
        for (int y = 0; y < out.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
            for (int x = 0; x < out.width(); ++x)
                line[x] = (line[x] & 0xff000000u) | (~line[x] & 0x00ffffffu);
        }
        return out;
    });
}

QImage brightnessContrast(const QImage &img, int brightness, int contrast, const QRect &rect)
{
    return applyRegion(img, rect, [brightness, contrast](const QImage &im) {
        Pixels a = toPixels(im);
        const double factor = (259.0 * (contrast * 2.55 + 255.0)) / (255.0 * (259.0 - contrast * 2.55));
        for (size_t i = 0; i < a.v.size(); i += 4) {
            for (size_t c = 0; c < 3; ++c) {
                const double v = a.v[i + c] + brightness * 2.55;
                a.v[i + c] = factor * (v - 128.0) + 128.0;
            }
        }
        return fromPixels(a);
    });
}

QImage hueSaturation(const QImage &img, int hue, int saturation, int lightness, const QRect &rect)
{
    return applyRegion(img, rect, [hue, saturation, lightness](const QImage &im) {
        Pixels a = toPixels(im);
        for (size_t i = 0; i < a.v.size(); i += 4) {
            double *px = a.v.data() + i;
            const double b = px[0] / 255.0;
            const double g = px[1] / 255.0;
            const double r = px[2] / 255.0;
            const double alpha = px[3] / 255.0;
            const double mx = std::max({b, g, r});
            const double mn = std::min({b, g, r});
            double light = (mx + mn) / 2.0;
            const double d = mx - mn;
            double s = d == 0.0 ? 0.0 : d / (1.0 - std::abs(2.0 * light - 1.0) + 1e-12);
            double h = 0.0;
            if (d != 0.0) {
                if (mx == r)
                    h = floorMod((g - b) / d, 6.0);
                else if (mx == g)
                    h = (b - r) / d + 2.0;
                else
                    h = (r - g) / d + 4.0;
            }
            h = floorMod(h * 60.0 + hue, 360.0);
            s = std::clamp(s * (1.0 + saturation / 100.0), 0.0, 1.0);
            light = std::clamp(light + lightness / 200.0, 0.0, 1.0);
            const double c = (1.0 - std::abs(2.0 * light - 1.0)) * s;
            const double x = c * (1.0 - std::abs(floorMod(h / 60.0, 2.0) - 1.0));
            const double m = light - c / 2.0;
            const int idx = ((int(std::floor(h / 60.0)) % 6) + 6) % 6;
            double rr = 0, gg = 0, bb = 0;
            switch (idx) {
            case 0: rr = c; gg = x; bb = 0; break;
            case 1: rr = x; gg = c; bb = 0; break;
            case 2: rr = 0; gg = c; bb = x; break;
            case 3: rr = 0; gg = x; bb = c; break;
            case 4: rr = x; gg = 0; bb = c; break;
            default: rr = c; gg = 0; bb = x; break;
            }
            px[2] = (rr + m) * 255.0;
            px[1] = (gg + m) * 255.0;
            px[0] = (bb + m) * 255.0;
            px[3] = alpha * 255.0;
        }
        return fromPixels(a);
    });
}

QImage rotate(const QImage &img, int degrees)
{
    QTransform t;
    t.rotate(degrees);
    return img.transformed(t, Qt::SmoothTransformation);
}

QImage flip(const QImage &img, bool horizontal)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return img.flipped(horizontal ? Qt::Horizontal : Qt::Vertical);
#else
    return img.mirrored(horizontal, !horizontal);
#endif
}

QImage resize(const QImage &img, int width, int height, bool smooth)
{
    return img.scaled(width, height, Qt::IgnoreAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
}

QImage canvasSize(const QImage &img, int width, int height, const QString &anchor, const QColor &fill)
{
    QImage out(width, height, QImage::Format_ARGB32);
    out.fill(fill.isValid() ? fill : QColor(Qt::white));
    QString ax = QStringLiteral("center");
    QString ay = QStringLiteral("center");
    if (anchor.contains(QLatin1Char('-'))) {
        ay = anchor.section(QLatin1Char('-'), 0, 0);
        ax = anchor.section(QLatin1Char('-'), 1);
    } else if (anchor == QLatin1String("left") || anchor == QLatin1String("center") || anchor == QLatin1String("right")) {
        ax = anchor;
    }
    int dx = floorDiv2(width - img.width());
    if (ax == QLatin1String("left"))
        dx = 0;
    else if (ax == QLatin1String("right"))
        dx = width - img.width();
    int dy = floorDiv2(height - img.height());
    if (ay == QLatin1String("top"))
        dy = 0;
    else if (ay == QLatin1String("bottom"))
        dy = height - img.height();
    QPainter painter(&out);
    painter.drawImage(QPoint(dx, dy), img);
    painter.end();
    return out;
}

QImage border(const QImage &img, int width, const QColor &color, bool inside)
{
    const QColor c = color.isValid() ? color : QColor(Qt::black);
    if (inside) {
        QImage out = argb(img);
        QPainter painter(&out);
        QPen pen(c, width);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        const double half = width / 2.0;
        painter.drawRect(QRectF(half, half, img.width() - width, img.height() - width));
        painter.end();
        return out;
    }
    QImage out(img.width() + width * 2, img.height() + width * 2, QImage::Format_ARGB32);
    out.fill(c);
    QPainter painter(&out);
    painter.drawImage(QPoint(width, width), img);
    painter.end();
    return out;
}

QImage dropShadow(const QImage &img, int offset, int radius, const QColor &bg)
{
    const int pad = offset + radius * 3;
    const int w = img.width() + pad;
    const int h = img.height() + pad;
    Pixels shadow(w, h);
    for (int y = offset; y < std::min(h, offset + img.height()); ++y) {
        for (int x = offset; x < std::min(w, offset + img.width()); ++x)
            shadow.px(x, y)[3] = 150.0;
    }
    for (int i = 0; i < 3; ++i) {
        boxBlurAxis(shadow, radius, 0);
        boxBlurAxis(shadow, radius, 1);
    }
    QImage out(w, h, QImage::Format_ARGB32);
    out.fill(bg.isValid() ? bg : QColor(0, 0, 0, 0));
    QPainter painter(&out);
    painter.drawImage(QPoint(0, 0), fromPixels(shadow));
    painter.drawImage(QPoint(0, 0), img);
    painter.end();
    return out;
}

QImage watermark(const QImage &img, const QString &text, const QColor &color, double opacity,
                 const QString &position, int size)
{
    QImage out = argb(img);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(opacity);
    QFont font;
    font.setPixelSize(size);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(color.isValid() ? color : QColor(Qt::white));
    const int margin = 12;
    if (position == QLatin1String("tile")) {
        const QFontMetrics metrics = painter.fontMetrics();
        const int tw = metrics.horizontalAdvance(text) + 60;
        const int th = metrics.height() + 60;
        painter.translate(out.width() / 2.0, out.height() / 2.0);
        painter.rotate(-30);
        const int diag = int(std::sqrt(double(out.width()) * out.width() + double(out.height()) * out.height()));
        for (int y = -diag; y < diag; y += th) {
            for (int x = -diag; x < diag; x += tw)
                painter.drawText(QPointF(x, y), text);
        }
    } else {
        Qt::Alignment flags = Qt::AlignBottom | Qt::AlignRight;
        if (position == QLatin1String("top-left"))
            flags = Qt::AlignTop | Qt::AlignLeft;
        else if (position == QLatin1String("top-right"))
            flags = Qt::AlignTop | Qt::AlignRight;
        else if (position == QLatin1String("bottom-left"))
            flags = Qt::AlignBottom | Qt::AlignLeft;
        else if (position == QLatin1String("center"))
            flags = Qt::AlignCenter;
        painter.drawText(out.rect().adjusted(margin, margin, -margin, -margin), int(flags), text);
    }
    painter.end();
    return out;
}

QImage fillRect(const QImage &img, const QRect &rect, const QColor &color)
{
    QImage out = argb(img);
    QPainter painter(&out);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, color);
    painter.end();
    return out;
}

QImage cropToPath(const QImage &img, const QPainterPath &path)
{
    const QRect rect = path.boundingRect().toAlignedRect().intersected(img.rect());
    QImage out(rect.size(), QImage::Format_ARGB32);
    out.fill(0);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-rect.topLeft());
    painter.setClipPath(path);
    painter.drawImage(QPoint(0, 0), img);
    painter.end();
    return out;
}

QImage floodFill(const QImage &img, int x, int y, const QColor &color, int tolerance)
{
    QImage out = argb(img);
    const int w = out.width();
    const int h = out.height();
    if (!(0 <= x && x < w && 0 <= y && y < h))
        return img;
    const QRgb target = reinterpret_cast<const QRgb *>(out.constScanLine(y))[x];
    std::vector<uint8_t> mask(size_t(w) * size_t(h), 0);
    for (int yy = 0; yy < h; ++yy) {
        const QRgb *line = reinterpret_cast<const QRgb *>(out.constScanLine(yy));
        uint8_t *m = mask.data() + size_t(yy) * size_t(w);
        for (int xx = 0; xx < w; ++xx) {
            const QRgb c = line[xx];
            const int diff = std::max({std::abs(qBlue(c) - qBlue(target)), std::abs(qGreen(c) - qGreen(target)),
                                       std::abs(qRed(c) - qRed(target)), std::abs(qAlpha(c) - qAlpha(target))});
            m[xx] = diff <= tolerance ? 1 : 0;
        }
    }
    std::vector<uint8_t> filled(size_t(w) * size_t(h), 0);
    std::vector<std::pair<int, int>> stack{{x, y}};
    while (!stack.empty()) {
        const auto [sx, sy] = stack.back();
        stack.pop_back();
        const size_t row = size_t(sy) * size_t(w);
        if (filled[row + size_t(sx)] || !mask[row + size_t(sx)])
            continue;
        int left = sx;
        while (left > 0 && mask[row + size_t(left - 1)])
            --left;
        int right = sx;
        while (right + 1 < w && mask[row + size_t(right + 1)])
            ++right;
        std::fill(filled.begin() + long(row) + left, filled.begin() + long(row) + right + 1, uint8_t(1));
        for (const int ny : {sy - 1, sy + 1}) {
            if (ny < 0 || ny >= h)
                continue;
            const size_t nrow = size_t(ny) * size_t(w);
            bool prev = false;
            for (int xx = left; xx <= right; ++xx) {
                const bool seg = mask[nrow + size_t(xx)] && !filled[nrow + size_t(xx)];
                if (seg && !prev)
                    stack.emplace_back(xx, ny);
                prev = seg;
            }
        }
    }
    const QRgb fillColor = qRgba(color.red(), color.green(), color.blue(), color.alpha());
    for (int yy = 0; yy < h; ++yy) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(yy));
        const uint8_t *f = filled.data() + size_t(yy) * size_t(w);
        for (int xx = 0; xx < w; ++xx) {
            if (f[xx])
                line[xx] = fillColor;
        }
    }
    return out;
}

} // namespace mm::editor::effects
