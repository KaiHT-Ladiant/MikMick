#pragma once

#include "core/AppContext.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>

#include <memory>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace mm {

namespace capture {
class CaptureManager;
}
namespace editor {
class EditorWindow;
}
namespace options {
class HotkeyManager;
}
namespace tools {
class ColorToolWindow;
}

class Controller : public QObject, public AppContext
{
    Q_OBJECT
public:
    explicit Controller(Config &config, QObject *parent = nullptr);
    ~Controller() override;

    // AppContext
    Config &config() override { return m_config; }
    void capture(const QString &mode, int delayMs = -1) override;
    void openTool(const QString &name) override;
    void showOptions(QWidget *parent) override;
    void shareImage(const QImage &image, const QString &target, QWidget *parent,
                    const QString &path = QString()) override;
    void homepage() override;
    void checkUpdates(bool manual, QWidget *parent) override;
    bool trayAvailable() const override;
    void notify(const QString &message) override;
    void quit() override;

    editor::EditorWindow *editor();
    // startPage: 1 = show start page, 0 = don't, -1 = only when configured and no document is open.
    void showEditor(int startPage = -1);
    // IPC / command line request: {files, capture, delay, tool, tray, options, quit}.
    void handleMessage(const QJsonObject &message);
    void dispatchAction(const QString &action);

private:
    void buildTray();
    void syncResultMenu();
    bool hideForCapture();
    void restoreAfterCapture();
    void onCaptured(const QImage &image, const QString &mode);
    void openToolNow(const QString &name, bool restoreEditor);
    tools::ColorToolWindow *colorWindow(bool paletteMode);
    QWidget *keep(QWidget *widget);
    void showError(const QString &message, QWidget *parent = nullptr);

    Config &m_config;
    QPointer<editor::EditorWindow> m_editor;
    capture::CaptureManager *m_capture = nullptr;
    options::HotkeyManager *m_hotkeys = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    std::unique_ptr<QMenu> m_trayMenu;
    QList<QPair<QString, QAction *>> m_resultActions;
    QList<QPointer<QWidget>> m_tools;
    QList<QPointer<QWidget>> m_hiddenForCapture;
};

} // namespace mm
