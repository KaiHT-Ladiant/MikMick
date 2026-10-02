#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>

namespace mm::options {

// Global hotkeys: XCB passive key grabs on X11, the xdg-desktop-portal GlobalShortcuts
// interface on Wayland.
class HotkeyManager : public QObject
{
    Q_OBJECT
public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    // action id -> "Shift+Print" (empty = unassigned). Replaces every previous registration.
    void apply(const QJsonObject &hotkeys);
    void stop();
    // Empty when everything registered. Portal failures arrive asynchronously (see errorChanged).
    QString error() const;
    bool supported() const;

Q_SIGNALS:
    void triggered(const QString &action);
    void errorChanged(const QString &error);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mm::options
