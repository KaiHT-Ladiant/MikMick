#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

namespace mm::sys {

QString autostartPath();
// Writes or removes ~/.config/autostart/mikmick.desktop.
void setAutostart(bool enabled, bool trayOnly = true);

QList<int> parseVersion(const QString &version);
bool isNewer(const QString &remote, const QString &local);

// GitHub latest release lookup. On success tag is non-empty.
using ReleaseCallback = std::function<void(const QString &tag, const QString &url)>;
void fetchLatestRelease(QObject *context, ReleaseCallback done);

bool isWayland();
bool isX11();

} // namespace mm::sys
