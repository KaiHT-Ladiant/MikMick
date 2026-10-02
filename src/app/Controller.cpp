#include "app/Controller.h"

#include "Version.h"
#include "capture/Backend.h"
#include "capture/CaptureManager.h"
#include "core/Catalog.h"
#include "core/Config.h"
#include "core/Outputs.h"
#include "core/System.h"
#include "editor/EditorWindow.h"
#include "options/HotkeyManager.h"
#include "options/OptionsDialog.h"
#include "tools/ColorTools.h"
#include "tools/ScreenTools.h"
#include "tools/Whiteboard.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace mm {

namespace {

const QString kGeneral = QStringLiteral("general");
const QString kCapture = QStringLiteral("capture");

QString appTitle()
{
    return QString::fromUtf8(kAppNameKo);
}

} // namespace

Controller::Controller(Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_capture = new capture::CaptureManager(
        m_config, [this] { return hideForCapture(); }, [this] { restoreAfterCapture(); }, this);
    connect(m_capture, &capture::CaptureManager::captured, this, &Controller::onCaptured);
    connect(m_capture, &capture::CaptureManager::failed, this, [this](const QString &msg) { showError(msg); });

    m_hotkeys = new options::HotkeyManager(this);
    connect(m_hotkeys, &options::HotkeyManager::triggered, this, &Controller::dispatchAction);
    m_hotkeys->apply(m_config.section(QStringLiteral("hotkeys")));

    buildTray();
}

Controller::~Controller()
{
    for (const QPointer<QWidget> &w : std::as_const(m_tools)) {
        if (w)
            delete w.data();
    }
    delete m_editor.data();
}

// ---------------------------------------------------------------- editor

editor::EditorWindow *Controller::editor()
{
    if (!m_editor)
        m_editor = new editor::EditorWindow(*this);
    return m_editor;
}

void Controller::showEditor(int startPage)
{
    editor::EditorWindow *ed = editor();
    bool start = startPage == 1;
    if (startPage < 0)
        start = !m_config.flag(kGeneral, QStringLiteral("hide_start_page"), false) && !ed->hasDocument();
    if (start)
        ed->showBackstage(QStringLiteral("start"));
    ed->bringToFront();
}

// ---------------------------------------------------------------- tray

bool Controller::trayAvailable() const
{
    return m_tray && QSystemTrayIcon::isSystemTrayAvailable();
}

void Controller::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(icons::icon(QStringLiteral("logo")), this);
    m_tray->setToolTip(appTitle() + QLatin1Char(' ') + QLatin1String(kVersion));

    m_trayMenu = std::make_unique<QMenu>();
    QMenu *menu = m_trayMenu.get();
    menu->addAction(icons::icon(QStringLiteral("tool_editor")), QStringLiteral("믹믹 에디터"), this,
                    [this] { showEditor(); });
    menu->addSeparator();
    for (const CaptureModeInfo &mode : captureModes()) {
        const QString id = mode.id;
        menu->addAction(icons::icon(mode.icon), mode.label, this, [this, id] { capture(id); });
    }
    QMenu *delayMenu = menu->addMenu(icons::icon(QStringLiteral("cap_delay")), QStringLiteral("지연 캡처 (영역 지정)"));
    for (int sec : {3, 5, 10}) {
        delayMenu->addAction(QStringLiteral("%1초 후").arg(sec), this,
                             [this, sec] { capture(QStringLiteral("region"), sec * 1000); });
    }
    menu->addSeparator();
    QMenu *toolsMenu = menu->addMenu(icons::icon(QStringLiteral("tool_color_picker")), QStringLiteral("그래픽 도구"));
    for (const ToolInfo &tool : graphicTools()) {
        const QString id = tool.id;
        toolsMenu->addAction(icons::icon(tool.icon), tool.title, this, [this, id] { openTool(id); });
    }
    QMenu *resultMenu = menu->addMenu(QStringLiteral("캡처 결과"));
    for (const auto &choice : resultChoices()) {
        QAction *a = resultMenu->addAction(choice.second);
        a->setCheckable(true);
        const QString key = choice.first;
        connect(a, &QAction::triggered, this, [this, key] {
            m_config.set(kCapture, QStringLiteral("result"), key);
            m_config.save();
        });
        m_resultActions.append({key, a});
    }
    connect(resultMenu, &QMenu::aboutToShow, this, &Controller::syncResultMenu);
    menu->addSeparator();
    menu->addAction(icons::icon(QStringLiteral("options")), QStringLiteral("옵션"), this,
                    [this] { showOptions(nullptr); });
    menu->addAction(icons::icon(QStringLiteral("info")), QStringLiteral("정보"), this, [this] {
        showEditor(0);
        editor()->showBackstage(QStringLiteral("info"));
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("종료"), this, &Controller::quit);

    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showEditor();
    });
    m_tray->show();
}

void Controller::syncResultMenu()
{
    const QString current = m_config.str(kCapture, QStringLiteral("result"), QStringLiteral("editor"));
    for (const auto &entry : std::as_const(m_resultActions))
        entry.second->setChecked(entry.first == current);
}

void Controller::notify(const QString &message)
{
    if (m_tray && m_tray->supportsMessages()) {
        m_tray->showMessage(appTitle(), message, icons::icon(QStringLiteral("logo")), 3000);
    } else if (m_editor && m_editor->isVisible()) {
        m_editor->statusBar()->showMessage(message, 4000);
    }
}

// ---------------------------------------------------------------- capture

void Controller::capture(const QString &mode, int delayMs)
{
    if (!isCaptureMode(mode))
        return;
    m_capture->capture(mode, delayMs);
}

bool Controller::hideForCapture()
{
    m_hiddenForCapture.clear();
    if (m_editor && m_editor->isVisible() && m_config.flag(QStringLiteral("editor"), QStringLiteral("hide_on_capture"), true))
        m_hiddenForCapture.append(m_editor.data());
    for (const QPointer<QWidget> &w : std::as_const(m_tools)) {
        if (w && w->isVisible())
            m_hiddenForCapture.append(w);
    }
    for (const QPointer<QWidget> &w : std::as_const(m_hiddenForCapture))
        w->hide();
    return !m_hiddenForCapture.isEmpty();
}

void Controller::restoreAfterCapture()
{
    bool editorHidden = false;
    for (const QPointer<QWidget> &w : std::as_const(m_hiddenForCapture)) {
        if (!w)
            continue;
        if (w == m_editor.data())
            editorHidden = true;
        else
            w->show();
    }
    if (editorHidden && m_config.str(kCapture, QStringLiteral("result"), QStringLiteral("editor")) != QLatin1String("editor"))
        m_editor->show();
    m_hiddenForCapture.clear();
}

void Controller::onCaptured(const QImage &image, const QString &mode)
{
    QString savedPath;
    if (m_config.flag(kCapture, QStringLiteral("always_clipboard"), false))
        outputs::copyToClipboard(image);
    if (m_config.flag(QStringLiteral("autosave"), QStringLiteral("enabled"), false))
        savedPath = outputs::autosave(image, m_config);

    const QString result = m_config.str(kCapture, QStringLiteral("result"), QStringLiteral("editor"));
    if (result == QLatin1String("editor")) {
        showEditor(0);
        editor()->addImage(image, QString(), captureModeLabel(mode));
        editor()->bringToFront();
    } else if (result == QLatin1String("clipboard")) {
        outputs::copyToClipboard(image);
        notify(QStringLiteral("캡처한 이미지를 클립보드에 복사했습니다."));
    } else if (result == QLatin1String("file")) {
        const outputs::GeneratedName n = outputs::nextFilename(m_config);
        QString folder = m_config.str(QStringLiteral("autosave"), QStringLiteral("folder"));
        if (folder.isEmpty())
            folder = picturesDir();
        const QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("다른 이름으로 저장"),
                                                          QDir(folder).filePath(n.name + QLatin1Char('.') + n.ext));
        if (!path.isEmpty() && outputs::saveImage(image, path, m_config))
            notify(QStringLiteral("저장됨: %1").arg(path));
    } else if (result == QLatin1String("autosave")) {
        const QString path = savedPath.isEmpty() ? outputs::autosave(image, m_config) : savedPath;
        if (!path.isEmpty())
            notify(QStringLiteral("자동 저장됨: %1").arg(path));
    } else if (result == QLatin1String("ftp") || result == QLatin1String("program")) {
        shareImage(image, result, nullptr, savedPath);
    }
}

// ---------------------------------------------------------------- share

void Controller::shareImage(const QImage &image, const QString &target, QWidget *parent, const QString &path)
{
    if (target == QLatin1String("clipboard")) {
        outputs::copyToClipboard(image);
        notify(QStringLiteral("클립보드에 복사했습니다."));
        return;
    }
    const QString filePath = (!path.isEmpty() && QFileInfo::exists(path)) ? path : outputs::tempSave(image, m_config);
    if (filePath.isEmpty()) {
        showError(QStringLiteral("임시 파일을 저장하지 못했습니다."), parent);
        return;
    }
    if (target == QLatin1String("email")) {
        if (!outputs::sendEmail(filePath))
            showError(QStringLiteral("xdg-email 을 실행할 수 없습니다."), parent);
    } else if (target == QLatin1String("default_app")) {
        outputs::openWithDefault(filePath);
    } else if (target == QLatin1String("program")) {
        if (m_config.str(QStringLiteral("autosave"), QStringLiteral("program")).isEmpty()) {
            showError(QStringLiteral("옵션 > 자동 저장 > 외부 프로그램 연결에서 프로그램을 먼저 지정하세요."), parent);
            return;
        }
        if (!outputs::runProgram(filePath, m_config))
            showError(QStringLiteral("외부 프로그램을 실행할 수 없습니다."), parent);
    } else if (target == QLatin1String("ftp")) {
        if (m_config.str(QStringLiteral("ftp"), QStringLiteral("server")).isEmpty()) {
            showError(QStringLiteral("옵션 > FTP 설정에서 서버를 먼저 지정하세요."), parent);
            return;
        }
        QPointer<QWidget> guard(parent);
        outputs::ftpUpload(filePath, m_config, this, [this, guard](bool ok, const QString &message) {
            if (ok)
                notify(QStringLiteral("FTP 전송 완료") + (message.isEmpty() ? QString() : QStringLiteral(": ") + message));
            else
                showError(QStringLiteral("FTP 전송 실패: %1").arg(message), guard);
        });
    }
}

// ---------------------------------------------------------------- tools

QWidget *Controller::keep(QWidget *widget)
{
    m_tools.removeAll(QPointer<QWidget>());
    m_tools.append(widget);
    return widget;
}

void Controller::openTool(const QString &name)
{
    if (name == QLatin1String("editor")) {
        showEditor();
        return;
    }
    if (!isTool(name))
        return;
    static const QStringList hidesEditor = {QStringLiteral("color_picker"), QStringLiteral("crosshair"),
                                            QStringLiteral("protractor"), QStringLiteral("whiteboard")};
    const bool hide = m_editor && m_editor->isVisible() && hidesEditor.contains(name);
    if (hide)
        m_editor->hide();
    QTimer::singleShot(hide ? 300 : 30, this, [this, name, hide] { openToolNow(name, hide); });
}

void Controller::openToolNow(const QString &name, bool restoreEditor)
{
    const bool multi = m_config.flag(kCapture, QStringLiteral("multi_monitor"), true);
    const int zoom = m_config.num(kCapture, QStringLiteral("magnifier_zoom"), 6);
    QPointer<editor::EditorWindow> ed = m_editor;
    auto restore = [ed, restoreEditor] {
        if (restoreEditor && ed)
            ed->show();
    };

    if (name == QLatin1String("color_picker") || name == QLatin1String("crosshair")
        || name == QLatin1String("protractor")) {
        const std::optional<capture::Snapshot> snap = capture::grabScreen(multi);
        if (!snap) {
            restore();
            showError(QStringLiteral("화면을 캡처할 수 없습니다."));
            return;
        }
        capture::FrozenOverlay *overlay = nullptr;
        if (name == QLatin1String("color_picker")) {
            auto *picker = new tools::ColorPickOverlay(*snap, qMax(zoom, 8));
            connect(picker, &tools::ColorPickOverlay::picked, this, [this, restore](const QColor &c) {
                restore();
                tools::ColorToolWindow *win = colorWindow(false);
                win->setColor(c);
                win->bringToFront();
                QGuiApplication::clipboard()->setText(win->codeText());
                notify(QStringLiteral("색상 %1 을(를) 클립보드에 복사했습니다.").arg(win->codeText()));
            });
            overlay = picker;
        } else if (name == QLatin1String("crosshair")) {
            overlay = new tools::CrosshairOverlay(*snap, zoom);
        } else {
            overlay = new tools::ProtractorOverlay(*snap, zoom);
        }
        connect(overlay, &capture::FrozenOverlay::cancelled, this, restore);
        keep(overlay);
        overlay->start();
    } else if (name == QLatin1String("palette")) {
        colorWindow(true)->bringToFront();
    } else if (name == QLatin1String("magnifier")) {
        std::optional<capture::Snapshot> snap;
        if (tools::needsSnapshotForLiveTools())
            snap = capture::grabScreen(multi);
        auto *mag = new tools::Magnifier(m_config, snap);
        keep(mag);
        mag->move(QCursor::pos() + QPoint(40, 40));
        mag->show();
    } else if (name == QLatin1String("ruler")) {
        auto *ruler = new tools::Ruler(m_config);
        keep(ruler);
        ruler->start();
    } else if (name == QLatin1String("whiteboard")) {
        std::optional<capture::Snapshot> snap;
        if (sys::isWayland())
            snap = capture::grabScreen(multi);
        auto *wb = new tools::Whiteboard(snap);
        connect(wb, &QObject::destroyed, this, restore);
        keep(wb);
        wb->start();
    }
}

tools::ColorToolWindow *Controller::colorWindow(bool paletteMode)
{
    for (const QPointer<QWidget> &w : std::as_const(m_tools)) {
        if (auto *win = qobject_cast<tools::ColorToolWindow *>(w.data()))
            return win;
    }
    auto *win = new tools::ColorToolWindow(m_config, paletteMode);
    QPointer<tools::ColorToolWindow> guard(win);
    connect(win, &tools::ColorToolWindow::pickAgain, this, [this, guard] {
        if (guard)
            guard->hide();
        QTimer::singleShot(250, this, [this] { openToolNow(QStringLiteral("color_picker"), false); });
    });
    keep(win);
    return win;
}

void Controller::dispatchAction(const QString &action)
{
    if (isCaptureMode(action))
        capture(action);
    else if (isTool(action))
        openTool(action);
}

// ---------------------------------------------------------------- options / about

void Controller::showOptions(QWidget *parent)
{
    options::OptionsDialog dlg(m_config, parent);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QJsonObject hotkeys = m_config.section(QStringLiteral("hotkeys"));
    m_hotkeys->apply(hotkeys);
    bool anyHotkey = false;
    for (auto it = hotkeys.begin(); it != hotkeys.end(); ++it)
        anyHotkey = anyHotkey || !it.value().toString().isEmpty();
    if (!m_hotkeys->error().isEmpty() && anyHotkey)
        notify(m_hotkeys->error());
    sys::setAutostart(m_config.flag(kGeneral, QStringLiteral("autostart"), false),
                      m_config.str(kGeneral, QStringLiteral("autostart_mode"), QStringLiteral("tray")) == QLatin1String("tray"));
    if (m_editor)
        m_editor->applySettings();
}

void Controller::homepage()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kRepoUrl)));
}

void Controller::checkUpdates(bool manual, QWidget *parent)
{
    QPointer<QWidget> guard(parent);
    sys::fetchLatestRelease(this, [guard, manual](const QString &tag, const QString &url) {
        QWidget *p = guard.data();
        const QString current = QString::fromLatin1(kVersion);
        if (tag.isEmpty()) {
            if (manual) {
                QMessageBox::information(p, appTitle(),
                                         QStringLiteral("업데이트 정보를 가져오지 못했습니다.\n"
                                                        "(아직 릴리스가 없거나 네트워크에 연결되지 않았습니다.)"));
            }
            return;
        }
        if (sys::isNewer(tag, current)) {
            const auto answer = QMessageBox::question(
                p, appTitle(),
                QStringLiteral("새 버전 %1 이(가) 있습니다. (현재 %2)\n다운로드 페이지를 여시겠습니까?").arg(tag, current));
            if (answer == QMessageBox::Yes) {
                const QString target = url.isEmpty() ? QString::fromLatin1(kRepoUrl) + QStringLiteral("/releases") : url;
                QDesktopServices::openUrl(QUrl(target));
            }
        } else if (manual) {
            QMessageBox::information(p, appTitle(), QStringLiteral("최신 버전(%1)을 사용 중입니다.").arg(current));
        }
    });
}

void Controller::showError(const QString &message, QWidget *parent)
{
    if (!parent && m_editor && m_editor->isVisible())
        parent = m_editor;
    QMessageBox::warning(parent, appTitle(), message);
}

void Controller::quit()
{
    if (m_editor && !m_editor->closeAll())
        return;
    m_hotkeys->stop();
    if (m_tray)
        m_tray->hide();
    QApplication::quit();
}

// ---------------------------------------------------------------- IPC

void Controller::handleMessage(const QJsonObject &message)
{
    const QString mode = message.value(QStringLiteral("capture")).toString();
    const QString tool = message.value(QStringLiteral("tool")).toString();
    if (!mode.isEmpty()) {
        const QJsonValue delay = message.value(QStringLiteral("delay"));
        capture(mode, delay.isDouble() ? delay.toInt() : -1);
    } else if (!tool.isEmpty()) {
        openTool(tool);
    } else if (message.value(QStringLiteral("options")).toBool()) {
        showOptions(nullptr);
    } else if (message.value(QStringLiteral("quit")).toBool()) {
        quit();
    } else {
        const QJsonArray files = message.value(QStringLiteral("files")).toArray();
        if (!files.isEmpty()) {
            showEditor(0);
            for (const QJsonValue &f : files)
                editor()->openPath(f.toString());
        } else if (!message.value(QStringLiteral("tray")).toBool()) {
            showEditor();
        }
    }
}

} // namespace mm
