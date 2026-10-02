#pragma once

#include <QImage>
#include <QString>

class QWidget;

namespace mm {

class Config;

// Application services used by the editor, tools and options UI.
// Implemented by the Controller in src/app.
class AppContext
{
public:
    virtual ~AppContext() = default;

    virtual Config &config() = 0;

    // mode: one of captureModeIds(); delayMs < 0 uses the configured delay.
    virtual void capture(const QString &mode, int delayMs = -1) = 0;
    // name: one of toolIds() ("editor", "color_picker", ...).
    virtual void openTool(const QString &name) = 0;
    virtual void showOptions(QWidget *parent) = 0;
    // target: "clipboard", "email", "default_app", "program", "ftp".
    // path is the document's file on disk, if it has one.
    virtual void shareImage(const QImage &image, const QString &target, QWidget *parent,
                            const QString &path = QString()) = 0;
    virtual void homepage() = 0;
    virtual void checkUpdates(bool manual, QWidget *parent) = 0;
    virtual bool trayAvailable() const = 0;
    virtual void notify(const QString &message) = 0;
    virtual void quit() = 0;
};

} // namespace mm
