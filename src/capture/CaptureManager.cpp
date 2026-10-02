#include "capture/CaptureManager.h"

#include "capture/CaptureOverlay.h"
#include "capture/ScrollCapture.h"
#include "core/Catalog.h"
#include "core/Config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QJsonArray>
#include <QPainter>
#include <QPolygonF>
#include <QProcess>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>

namespace mm::capture {

namespace {

const QString kCapture = QStringLiteral("capture");
const QString kRecent = QStringLiteral("recent");

} // namespace

void drawCursor(QImage &image, const QPoint &pos)
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(pos);
    const QPolygonF poly{QPointF(0, 0),   QPointF(0, 17),  QPointF(4, 13), QPointF(7, 20),
                         QPointF(10, 19), QPointF(7, 12), QPointF(12, 12)};
    painter.setPen(QPen(Qt::black, 1.2));
    painter.setBrush(Qt::white);
    painter.drawPolygon(poly);
}

void playShutter()
{
    const QString sound = QStringLiteral("/usr/share/sounds/freedesktop/stereo/screen-capture.oga");
    QList<QStringList> commands;
    if (hasTool(QStringLiteral("canberra-gtk-play")))
        commands << QStringList{QStringLiteral("canberra-gtk-play"), QStringLiteral("-i"),
                                QStringLiteral("screen-capture")};
    if (hasTool(QStringLiteral("paplay")) && QFile::exists(sound))
        commands << QStringList{QStringLiteral("paplay"), sound};
    for (const QStringList &cmd : std::as_const(commands)) {
        QProcess proc;
        proc.setProgram(cmd.first());
        proc.setArguments(cmd.mid(1));
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        if (proc.startDetached())
            return;
    }
    QApplication::beep();
}

FixedSizeDialog::FixedSizeDialog(const QSize &size, QWidget *parent)
    : QDialog(parent)
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
{
    setWindowTitle(QStringLiteral("고정된 사각 영역 크기"));
    auto *form = new QFormLayout(this);
    m_width->setRange(10, 10000);
    m_width->setValue(size.width());
    m_height->setRange(10, 10000);
    m_height->setValue(size.height());
    form->addRow(QStringLiteral("가로 (Width)"), m_width);
    form->addRow(QStringLiteral("세로 (Height)"), m_height);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

QSize FixedSizeDialog::value() const
{
    return QSize(m_width->value(), m_height->value());
}

CaptureManager::CaptureManager(Config &config, std::function<bool()> hideWindows,
                               std::function<void()> restoreWindows, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_hideWindows(std::move(hideWindows))
    , m_restoreWindows(std::move(restoreWindows))
{
}

void CaptureManager::capture(const QString &mode, int delayMs)
{
    if (m_busy)
        return;
    if (mode == QLatin1String("fixed")) {
        const QSize size(m_config.num(kCapture, QStringLiteral("fixed_width"), 640),
                         m_config.num(kCapture, QStringLiteral("fixed_height"), 480));
        FixedSizeDialog dlg(size);
        if (dlg.exec() != QDialog::Accepted)
            return;
        m_config.set(kCapture, QStringLiteral("fixed_width"), dlg.value().width());
        m_config.set(kCapture, QStringLiteral("fixed_height"), dlg.value().height());
        m_config.save();
    }
    m_busy = true;
    const bool hidden = m_hideWindows ? m_hideWindows() : false;
    const int delay = delayMs < 0 ? m_config.num(kCapture, QStringLiteral("delay_ms"), 0) : delayMs;
    const int wait = qMax(delay, hidden ? 350 : 80);
    QTimer::singleShot(wait, this, [this, mode]() { run(mode); });
}

std::optional<Snapshot> CaptureManager::snapshot()
{
    auto snap = grabScreen(m_config.flag(kCapture, QStringLiteral("multi_monitor"), true));
    if (!snap)
        fail(QStringLiteral("화면을 캡처할 수 없습니다.\nWayland 환경이라면 xdg-desktop-portal 또는 grim 을 설치하세요."));
    return snap;
}

void CaptureManager::run(QString mode)
{
    m_cursor = QCursor::pos();
    if (mode == QLatin1String("repeat_last")) {
        const QJsonArray last = m_config.value(kRecent, QStringLiteral("last_region")).toArray();
        bool valid = last.size() == 4;
        for (const QJsonValue &v : last)
            valid = valid && v.isDouble();
        const QRect rect = valid ? QRect(last[0].toInt(), last[1].toInt(), last[2].toInt(), last[3].toInt())
                                 : QRect();
        if (!rect.isEmpty()) {
            const auto snap = snapshot();
            if (!snap)
                return;
            const QRect clipped = rect.intersected(snap->geometry);
            if (!clipped.isEmpty()) {
                deliver(snap->crop(clipped).toImage(), clipped, mode);
                return;
            }
        }
        mode = QStringLiteral("region");
    }

    QString overlayMode = mode;
    if (mode == QLatin1String("active_window")) {
        const auto info = activeWindow();
        if (info && info->pid != QCoreApplication::applicationPid()) {
            const auto snap = snapshot();
            if (!snap)
                return;
            const QRect rect = info->rect.intersected(snap->geometry);
            if (!rect.isEmpty()) {
                deliver(snap->crop(rect).toImage(), rect, mode);
                return;
            }
        }
        overlayMode = QStringLiteral("window");
    } else if (mode == QLatin1String("window_control") || mode == QLatin1String("scroll")) {
        overlayMode = QStringLiteral("window");
    } else if (mode == QLatin1String("fullscreen")) {
        const auto snap = snapshot();
        if (!snap)
            return;
        deliver(snap->crop(snap->geometry).toImage(), snap->geometry, mode);
        return;
    }

    const auto snap = snapshot();
    if (!snap)
        return;
    const QList<WindowInfo> windows = overlayMode == QLatin1String("window") ? windowList() : QList<WindowInfo>();
    auto *overlay = new CaptureOverlay(
        *snap, overlayMode, m_config.flag(kCapture, QStringLiteral("magnifier"), true),
        m_config.num(kCapture, QStringLiteral("magnifier_zoom"), 6),
        QSize(m_config.num(kCapture, QStringLiteral("fixed_width"), 640),
              m_config.num(kCapture, QStringLiteral("fixed_height"), 480)),
        windows, m_config.flag(kCapture, QStringLiteral("show_toolbar"), true));
    overlay->setWindowTitle(captureModeLabel(mode));
    const Snapshot frozen = *snap;
    connect(overlay, &CaptureOverlay::selected, this,
            [this, frozen, mode](const QRect &rect, const QPainterPath &path) { onSelected(frozen, rect, path, mode); });
    connect(overlay, &CaptureOverlay::cancelled, this, &CaptureManager::onCancelled);
    connect(overlay, &QObject::destroyed, this, [this]() {
        if (m_awaitingOverlay)
            onCancelled();
    });
    m_overlay = overlay;
    m_awaitingOverlay = true;
    overlay->start();
}

void CaptureManager::onSelected(const Snapshot &snap, const QRect &rect, const QPainterPath &path,
                                const QString &mode)
{
    m_awaitingOverlay = false;
    m_overlay = nullptr;
    if (mode == QLatin1String("scroll")) {
        QString msg;
        if (!ScrollCapture::supported(&msg)) {
            deliver(snap.crop(rect).toImage(), rect, mode);
            Q_EMIT failed(msg + QStringLiteral("\n선택한 영역만 캡처했습니다."));
            return;
        }
        if (m_scroll)
            m_scroll->deleteLater();
        auto *scroll = new ScrollCapture(rect, m_config.num(kCapture, QStringLiteral("scroll_delay_ms"), 100), 60, this);
        connect(scroll, &ScrollCapture::finished, this, [this, scroll, rect, mode](const QImage &image) {
            scroll->deleteLater();
            deliver(image, rect, mode, false);
        });
        connect(scroll, &ScrollCapture::failed, this, [this, scroll](const QString &message) {
            scroll->deleteLater();
            fail(message);
        });
        m_scroll = scroll;
        QTimer::singleShot(150, scroll, &ScrollCapture::start);
        return;
    }
    QImage image = snap.crop(rect).toImage();
    if (!path.isEmpty()) {
        QImage masked(image.size(), QImage::Format_ARGB32_Premultiplied);
        masked.fill(Qt::transparent);
        QPainter painter(&masked);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipPath(path);
        painter.drawImage(0, 0, image);
        painter.end();
        image = masked;
    }
    deliver(image, rect, mode, path.isEmpty());
}

void CaptureManager::deliver(QImage image, const QRect &rect, const QString &mode, bool remember)
{
    if (remember && mode != QLatin1String("fullscreen")) {
        m_config.set(kRecent, QStringLiteral("last_region"),
                     QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()});
        if (mode != QLatin1String("repeat_last"))
            m_config.set(kRecent, QStringLiteral("last_mode"), QJsonValue(mode));
        m_config.save();
    }
    if (m_config.flag(kCapture, QStringLiteral("include_cursor"), false) && mode != QLatin1String("scroll")) {
        const QPoint local = m_cursor - rect.topLeft();
        if (QRect(QPoint(0, 0), rect.size()).contains(local)) {
            const double sx = image.width() / double(qMax(1, rect.width()));
            const double sy = image.height() / double(qMax(1, rect.height()));
            drawCursor(image, QPoint(qRound(local.x() * sx), qRound(local.y() * sy)));
        }
    }
    if (m_config.flag(kCapture, QStringLiteral("sound"), true))
        playShutter();
    m_busy = false;
    if (m_restoreWindows)
        m_restoreWindows();
    Q_EMIT captured(image, mode);
}

void CaptureManager::onCancelled()
{
    m_awaitingOverlay = false;
    m_overlay = nullptr;
    m_busy = false;
    if (m_restoreWindows)
        m_restoreWindows();
}

void CaptureManager::fail(const QString &message)
{
    m_busy = false;
    if (m_restoreWindows)
        m_restoreWindows();
    Q_EMIT failed(message);
}

} // namespace mm::capture
