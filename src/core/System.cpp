#include "core/System.h"

#include "Version.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>

namespace mm::sys {

QString autostartPath()
{
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/autostart/mikmick.desktop");
}

static QString launcherCommand()
{
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("mikmick"));
    if (!exe.isEmpty())
        return exe;
    return QCoreApplication::applicationFilePath();
}

void setAutostart(bool enabled, bool trayOnly)
{
    const QString path = autostartPath();
    if (!enabled) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const QString args = trayOnly ? QStringLiteral(" --tray") : QString();
    const QString text = QStringLiteral("[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=%1\n"
                                        "Name[ko]=믹믹\n"
                                        "Comment=Screen capture and image editor\n"
                                        "Exec=%2%3\n"
                                        "Icon=mikmick\n"
                                        "Terminal=false\n"
                                        "X-GNOME-Autostart-enabled=true\n")
                             .arg(QLatin1String(kAppName), launcherCommand(), args);
    file.write(text.toUtf8());
}

QList<int> parseVersion(const QString &version)
{
    QString v = version.trimmed();
    while (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V')))
        v.remove(0, 1);
    QList<int> out;
    for (const QString &part : v.split(QLatin1Char('.'))) {
        QString digits;
        for (const QChar ch : part) {
            if (!ch.isDigit())
                break;
            digits += ch;
        }
        out << (digits.isEmpty() ? 0 : digits.toInt());
    }
    return out;
}

bool isNewer(const QString &remote, const QString &local)
{
    QList<int> a = parseVersion(remote);
    QList<int> b = parseVersion(local);
    const int n = qMax(a.size(), b.size());
    while (a.size() < n)
        a << 0;
    while (b.size() < n)
        b << 0;
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i])
            return a[i] > b[i];
    }
    return false;
}

void fetchLatestRelease(QObject *context, ReleaseCallback done)
{
    auto *nam = new QNetworkAccessManager(context);
    QNetworkRequest req{QUrl(QString::fromLatin1(kReleasesApi))};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", QByteArray("MikMick/") + kVersion);
    req.setTransferTimeout(8000);
    QPointer<QObject> guard(context);
    QNetworkReply *reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, nam, [reply, nam, done, guard]() {
        QString tag;
        QString url;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object();
            tag = data.value(QStringLiteral("tag_name")).toString();
            url = data.value(QStringLiteral("html_url")).toString();
        }
        reply->deleteLater();
        nam->deleteLater();
        if (guard)
            done(tag, url);
    });
}

bool isWayland()
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
        return true;
    return qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")
        || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
}

bool isX11()
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("xcb"))
        return true;
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))
        return false;
    return !isWayland() && !qEnvironmentVariable("DISPLAY").isEmpty();
}

} // namespace mm::sys
