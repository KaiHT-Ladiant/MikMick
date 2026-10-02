#include "capture/Backend.h"

#include "capture/X11.h"
#include "core/System.h"

#include <QCursor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#ifdef MIKMICK_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#endif

namespace mm::capture {

namespace {

class PortalRequest : public QObject
{
    Q_OBJECT
public:
    QEventLoop loop;
    bool done = false;
    uint response = 2;
    QVariantMap results;

public Q_SLOTS:
    void onResponse(uint code, const QVariantMap &values)
    {
        done = true;
        response = code;
        results = values;
        loop.quit();
    }
};

QPixmap grabQt(const QRect &geometry)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return {};
    QPixmap pm = screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    if (pm.isNull())
        return {};
    pm.setDevicePixelRatio(1.0);
    return pm;
}

QString portalScreenshot(bool interactive)
{
#ifdef MIKMICK_HAVE_DBUS
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return {};
    QString token = QStringLiteral("mikmick_");
    for (int i = 0; i < 8; ++i)
        token += QChar(QLatin1Char(char('a' + QRandomGenerator::global()->bounded(26))));
    const QString sender = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString iface = QStringLiteral("org.freedesktop.portal.Request");
    const QString member = QStringLiteral("Response");
    QStringList handles{QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token)};

    PortalRequest request;
    auto listen = [&](const QString &path) {
        bus.connect(QString(), path, iface, member, &request, SLOT(onResponse(uint,QVariantMap)));
    };
    listen(handles.first());

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                       QStringLiteral("/org/freedesktop/portal/desktop"),
                                                       QStringLiteral("org.freedesktop.portal.Screenshot"),
                                                       QStringLiteral("Screenshot"));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("interactive"), interactive);
    call << QString() << options;
    const QDBusReply<QDBusObjectPath> reply = bus.call(call, QDBus::Block, 10000);
    if (reply.isValid() && !request.done) {
        // Portals older than 0.9 ignore handle_token and return their own request path.
        const QString actual = reply.value().path();
        if (!actual.isEmpty() && actual != handles.first()) {
            handles << actual;
            listen(actual);
        }
        QTimer::singleShot(120000, &request.loop, &QEventLoop::quit);
        request.loop.exec();
    }
    for (const QString &path : std::as_const(handles))
        bus.disconnect(QString(), path, iface, member, &request, SLOT(onResponse(uint,QVariantMap)));

    if (!request.done || request.response != 0)
        return {};
    const QString uri = request.results.value(QStringLiteral("uri")).toString();
    if (uri.isEmpty())
        return {};
    return QUrl(uri).toLocalFile();
#else
    Q_UNUSED(interactive);
    return {};
#endif
}

QString externalScreenshot()
{
    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/mikmick-XXXXXX.png"));
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {};
    const QString path = tmp.fileName();
    tmp.close();

    QList<QStringList> commands;
    if (hasTool(QStringLiteral("grim")))
        commands << QStringList{QStringLiteral("grim"), path};
    if (hasTool(QStringLiteral("gnome-screenshot")))
        commands << QStringList{QStringLiteral("gnome-screenshot"), QStringLiteral("-f"), path};
    if (hasTool(QStringLiteral("spectacle")))
        commands << QStringList{QStringLiteral("spectacle"), QStringLiteral("-b"), QStringLiteral("-n"),
                                QStringLiteral("-f"), QStringLiteral("-o"), path};
    if (hasTool(QStringLiteral("scrot")))
        commands << QStringList{QStringLiteral("scrot"), QStringLiteral("-o"), path};
    if (hasTool(QStringLiteral("maim")))
        commands << QStringList{QStringLiteral("maim"), path};

    for (const QStringList &cmd : std::as_const(commands)) {
        QProcess proc;
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        proc.start(cmd.first(), cmd.mid(1));
        if (!proc.waitForStarted(5000))
            continue;
        if (!proc.waitForFinished(30000)) {
            proc.kill();
            proc.waitForFinished(1000);
            continue;
        }
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 && QFileInfo(path).size() > 0)
            return path;
    }
    QFile::remove(path);
    return {};
}

} // namespace

QPixmap Snapshot::crop(const QRect &rect) const
{
    const QRect local = rect.translated(-geometry.topLeft());
    const double sx = scaleX();
    const double sy = scaleY();
    const QRect src(qRound(local.x() * sx), qRound(local.y() * sy), qRound(local.width() * sx),
                    qRound(local.height() * sy));
    QPixmap out = pixmap.copy(src);
    out.setDevicePixelRatio(1.0);
    return out;
}

bool hasTool(const QString &name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

QRect virtualGeometry(bool multiMonitor)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return QRect(0, 0, 1920, 1080);
    if (!multiMonitor) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        return screen ? screen->geometry() : screens.first()->geometry();
    }
    QRect rect;
    for (const QScreen *s : screens)
        rect = rect.united(s->geometry());
    return rect;
}

std::optional<Snapshot> grabScreen(bool multiMonitor)
{
    const QRect geometry = virtualGeometry(multiMonitor);
    if (!sys::isWayland()) {
        const QPixmap pm = grabQt(geometry);
        if (!pm.isNull())
            return Snapshot{pm, geometry};
    }
    QString path = portalScreenshot(false);
    if (path.isEmpty())
        path = externalScreenshot();
    if (!path.isEmpty()) {
        QPixmap pm(path);
        QFile::remove(path);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(1.0);
            const QRect full = virtualGeometry(true);
            const Snapshot snap{pm, full};
            if (!multiMonitor && full != geometry)
                return Snapshot{snap.crop(geometry), geometry};
            return snap;
        }
    }
    const QPixmap pm = grabQt(geometry);
    if (!pm.isNull())
        return Snapshot{pm, geometry};
    return std::nullopt;
}

QPixmap grabLive(const QRect &rect)
{
    if (sys::isWayland())
        return {};
    QScreen *screen = QGuiApplication::screenAt(rect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return {};
    return screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height());
}

std::optional<WindowInfo> activeWindow()
{
    return x11::activeWindow();
}

QList<WindowInfo> windowList()
{
    return x11::windowList();
}

} // namespace mm::capture

#include "Backend.moc"
