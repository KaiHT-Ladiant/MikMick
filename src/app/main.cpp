#include "Version.h"
#include "app/Controller.h"
#include "core/Catalog.h"
#include "core/Config.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

#include <cstdio>

namespace {

QString serverName()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = QStringLiteral("user");
    return QStringLiteral("mikmick-") + user;
}

bool sendToRunning(const QJsonObject &message)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(400))
        return false;
    socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

[[noreturn]] void failUsage(QCommandLineParser &parser, const QString &message)
{
    std::fprintf(stderr, "%s\n\n%s", qPrintable(message), qPrintable(parser.helpText()));
    std::exit(2);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setDesktopFileName(QString::fromLatin1(mm::kAppId));
    QApplication app(argc, argv);
    QApplication::setApplicationName(QString::fromLatin1(mm::kAppName));
    QApplication::setApplicationDisplayName(QString::fromUtf8(mm::kAppNameKo));
    QApplication::setApplicationVersion(QString::fromLatin1(mm::kVersion));
    QApplication::setOrganizationName(QString::fromLatin1(mm::kAppName));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon::hasThemeIcon(QStringLiteral("mikmick"))
                                    ? QIcon::fromTheme(QStringLiteral("mikmick"))
                                    : mm::icons::icon(QStringLiteral("logo")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("믹믹(MikMick) - 리눅스용 화면 캡처 & 이미지 편집 도구"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("에디터로 열 이미지 파일"),
                                 QStringLiteral("[files...]"));
    const QCommandLineOption captureOpt({QStringLiteral("c"), QStringLiteral("capture")},
                                        QStringLiteral("캡처 모드 실행 (%1)").arg(mm::captureModeIds().join(QStringLiteral(", "))),
                                        QStringLiteral("mode"));
    const QCommandLineOption delayOpt({QStringLiteral("d"), QStringLiteral("delay")}, QStringLiteral("캡처 지연 (ms)"),
                                      QStringLiteral("ms"));
    const QCommandLineOption toolOpt({QStringLiteral("t"), QStringLiteral("tool")},
                                     QStringLiteral("그래픽 도구 실행 (%1)").arg(mm::toolIds().join(QStringLiteral(", "))),
                                     QStringLiteral("name"));
    const QCommandLineOption trayOpt(QStringLiteral("tray"), QStringLiteral("알림 영역에만 표시하여 시작"));
    const QCommandLineOption optionsOpt(QStringLiteral("options"), QStringLiteral("옵션 대화상자 열기"));
    const QCommandLineOption quitOpt(QStringLiteral("quit"), QStringLiteral("실행 중인 믹믹 종료"));
    parser.addOptions({captureOpt, delayOpt, toolOpt, trayOpt, optionsOpt, quitOpt});
    parser.process(app);

    const QString mode = parser.value(captureOpt);
    const QString tool = parser.value(toolOpt);
    if (!mode.isEmpty() && !mm::isCaptureMode(mode))
        failUsage(parser, QStringLiteral("알 수 없는 캡처 모드: %1").arg(mode));
    if (!tool.isEmpty() && !mm::isTool(tool))
        failUsage(parser, QStringLiteral("알 수 없는 도구: %1").arg(tool));
    QJsonValue delay;
    if (parser.isSet(delayOpt)) {
        bool ok = false;
        const int ms = parser.value(delayOpt).toInt(&ok);
        if (!ok || ms < 0)
            failUsage(parser, QStringLiteral("지연 시간은 0 이상의 정수(ms)여야 합니다."));
        delay = ms;
    }

    QJsonArray files;
    for (const QString &f : parser.positionalArguments())
        files.append(QFileInfo(f).absoluteFilePath());

    QJsonObject message;
    message.insert(QStringLiteral("files"), files);
    message.insert(QStringLiteral("capture"), mode.isEmpty() ? QJsonValue() : QJsonValue(mode));
    message.insert(QStringLiteral("delay"), delay);
    message.insert(QStringLiteral("tool"), tool.isEmpty() ? QJsonValue() : QJsonValue(tool));
    message.insert(QStringLiteral("tray"), parser.isSet(trayOpt));
    message.insert(QStringLiteral("options"), parser.isSet(optionsOpt));
    message.insert(QStringLiteral("quit"), parser.isSet(quitOpt));

    if (sendToRunning(message) || parser.isSet(quitOpt))
        return 0;

    QLocalServer::removeServer(serverName());
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.listen(serverName());

    mm::Config config;
    mm::Controller controller(config);

    QObject::connect(&server, &QLocalServer::newConnection, &controller, [&server, &controller] {
        QLocalSocket *socket = server.nextPendingConnection();
        if (!socket)
            return;
        QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        if (socket->waitForReadyRead(1000)) {
            QByteArray data = socket->readAll();
            while (socket->waitForReadyRead(50))
                data += socket->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                const QJsonObject msg = doc.object();
                QTimer::singleShot(0, &controller, [&controller, msg] { controller.handleMessage(msg); });
            }
        }
        socket->disconnectFromServer();
    });

    const bool cliTask = !mode.isEmpty() || !tool.isEmpty() || parser.isSet(optionsOpt) || !files.isEmpty();
    if (cliTask) {
        QTimer::singleShot(0, &controller, [&controller, message] { controller.handleMessage(message); });
    } else {
        const bool trayOnly = parser.isSet(trayOpt)
            || config.str(QStringLiteral("general"), QStringLiteral("start_mode"), QStringLiteral("editor"))
                == QLatin1String("tray");
        if (trayOnly && controller.trayAvailable())
            controller.notify(QStringLiteral("믹믹이 알림 영역에서 실행 중입니다."));
        else
            controller.showEditor();
    }
    if (config.flag(QStringLiteral("general"), QStringLiteral("check_updates"), true))
        QTimer::singleShot(5000, &controller, [&controller] { controller.checkUpdates(false, nullptr); });
    // Without a tray, a CLI-started task should end the process once its windows close.
    if (!controller.trayAvailable() && (!mode.isEmpty() || !tool.isEmpty()))
        QApplication::setQuitOnLastWindowClosed(true);

    return app.exec();
}
