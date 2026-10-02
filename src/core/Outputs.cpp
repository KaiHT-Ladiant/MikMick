#include "core/Outputs.h"

#include "core/Config.h"
#include "core/Filename.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace mm::outputs {

bool saveImage(const QImage &image, const QString &path, const Config &config)
{
    QString ext = extOf(path);
    if (ext.isEmpty())
        ext = QStringLiteral("png");

    if (ext == QLatin1String("pdf")) {
        QPdfWriter writer(path);
        writer.setResolution(96);
        writer.setPageSize(QPageSize(QSizeF(image.width(), image.height()), QPageSize::Point));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter painter(&writer);
        const QRect target = painter.viewport();
        const QSize scaled = image.size().scaled(target.size(), Qt::KeepAspectRatio);
        painter.drawImage(QRect(target.topLeft(), scaled), image);
        painter.end();
        return QFileInfo::exists(path);
    }

    QByteArray fmt = "PNG";
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
        fmt = "JPEG";
    else if (ext == QLatin1String("bmp"))
        fmt = "BMP";
    else if (ext == QLatin1String("gif"))
        fmt = "GIF";
    else if (ext == QLatin1String("webp"))
        fmt = "WEBP";
    else if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
        fmt = "TIFF";

    const int quality = (fmt == "JPEG" || fmt == "WEBP")
        ? config.num(QStringLiteral("image"), QStringLiteral("jpeg_quality"), 100)
        : -1;
    if (fmt == "JPEG" || fmt == "BMP")
        return image.convertToFormat(QImage::Format_RGB32).save(path, fmt.constData(), quality);
    return image.save(path, fmt.constData(), quality);
}

GeneratedName nextFilename(Config &config)
{
    const int counter = config.num(QStringLiteral("filename"), QStringLiteral("counter"), 0);
    GeneratedName out;
    out.name = expandPattern(config.str(QStringLiteral("filename"), QStringLiteral("pattern"), QStringLiteral("%c")),
                             counter);
    out.ext = config.str(QStringLiteral("filename"), QStringLiteral("format"), QStringLiteral("png"));
    config.set(QStringLiteral("filename"), QStringLiteral("counter"), counter + 1);
    config.save();
    return out;
}

QString autosaveFolder(const Config &config)
{
    QString folder = config.str(QStringLiteral("autosave"), QStringLiteral("folder"));
    if (folder.isEmpty())
        folder = picturesDir() + QStringLiteral("/MikMick");
    QDir().mkpath(folder);
    return folder;
}

QString autosave(const QImage &image, Config &config)
{
    const GeneratedName n = nextFilename(config);
    const QString path = uniquePath(autosaveFolder(config), n.name, n.ext);
    return saveImage(image, path, config) ? path : QString();
}

QString tempSave(const QImage &image, Config &config)
{
    const GeneratedName n = nextFilename(config);
    const QString folder = QDir::tempPath() + QStringLiteral("/mikmick");
    QDir().mkpath(folder);
    const QString path = uniquePath(folder, n.name, n.ext);
    return saveImage(image, path, config) ? path : QString();
}

void copyToClipboard(const QImage &image)
{
    QGuiApplication::clipboard()->setImage(image);
}

bool runProgram(const QString &path, const Config &config)
{
    const QString program = config.str(QStringLiteral("autosave"), QStringLiteral("program")).trimmed();
    if (program.isEmpty())
        return false;
    const QString args = config.str(QStringLiteral("autosave"), QStringLiteral("program_args")).trimmed();
    QStringList argList = QProcess::splitCommand(args);
    if (args.contains(QLatin1String("%f"))) {
        for (QString &a : argList)
            a.replace(QLatin1String("%f"), path);
    } else {
        argList << path;
    }
    return QProcess::startDetached(program, argList);
}

void openWithDefault(const QString &path)
{
    if (!QProcess::startDetached(QStringLiteral("xdg-open"), {path}))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool sendEmail(const QString &path)
{
    return QProcess::startDetached(QStringLiteral("xdg-email"), {QStringLiteral("--attach"), path});
}

namespace {

QString ftpBaseUrl(const Config &config)
{
    QString server = config.str(QStringLiteral("ftp"), QStringLiteral("server")).trimmed();
    if (!server.contains(QLatin1String("://")))
        server.prepend(QStringLiteral("ftp://"));
    QUrl url(server);
    const int port = config.num(QStringLiteral("ftp"), QStringLiteral("port"), 21);
    if (url.port() < 0 && port > 0 && port != 21)
        url.setPort(port);
    QString remote = config.str(QStringLiteral("ftp"), QStringLiteral("path")).trimmed();
    if (!remote.startsWith(QLatin1Char('/')))
        remote.prepend(QLatin1Char('/'));
    if (!remote.endsWith(QLatin1Char('/')))
        remote.append(QLatin1Char('/'));
    url.setPath(remote);
    return url.toString();
}

QByteArray curlQuote(const QString &value)
{
    QString v = value;
    v.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    v.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return '"' + v.toUtf8() + '"';
}

// Credentials go through stdin (-K -) so they never show up in the process list.
void runCurl(const QStringList &args, const Config &config, QObject *context,
             std::function<void(bool, const QString &, const QByteArray &)> done)
{
    auto *proc = new QProcess(context);
    QPointer<QObject> guard(context);
    QObject::connect(proc, &QProcess::finished, proc, [proc, done, guard](int code, QProcess::ExitStatus status) {
        const QByteArray out = proc->readAllStandardOutput();
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        if (guard)
            done(status == QProcess::NormalExit && code == 0, err, out);
    });
    QObject::connect(proc, &QProcess::errorOccurred, proc, [proc, done, guard](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        if (guard)
            done(false, QStringLiteral("curl 을 실행할 수 없습니다. curl 패키지를 설치하세요."), {});
    });

    QStringList full{QStringLiteral("-sS"), QStringLiteral("--connect-timeout"), QStringLiteral("15"),
                     QStringLiteral("-K"), QStringLiteral("-")};
    if (!config.flag(QStringLiteral("ftp"), QStringLiteral("passive"), false))
        full << QStringLiteral("--ftp-port") << QStringLiteral("-");
    full << args;
    proc->start(QStringLiteral("curl"), full);

    QString user = config.str(QStringLiteral("ftp"), QStringLiteral("user"), QStringLiteral("anonymous"));
    if (user.isEmpty())
        user = QStringLiteral("anonymous");
    const QString password = config.str(QStringLiteral("ftp"), QStringLiteral("password"));
    proc->write("user = " + curlQuote(user + QLatin1Char(':') + password) + '\n');
    proc->closeWriteChannel();
}

} // namespace

void ftpUpload(const QString &path, const Config &config, QObject *context, FtpCallback done)
{
    const QString name = QFileInfo(path).fileName();
    const QString target = ftpBaseUrl(config) + QString::fromUtf8(QUrl::toPercentEncoding(name));
    QString base = config.str(QStringLiteral("ftp"), QStringLiteral("url")).trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString publicUrl = base.isEmpty() ? QString() : base + QLatin1Char('/') + name;
    const bool copyUrl = config.flag(QStringLiteral("ftp"), QStringLiteral("copy_url"), false);
    const bool openUrl = config.flag(QStringLiteral("ftp"), QStringLiteral("open_url"), false);

    runCurl({QStringLiteral("--ftp-create-dirs"), QStringLiteral("-T"), path, target}, config, context,
            [=](bool ok, const QString &err, const QByteArray &) {
                if (!ok) {
                    done(false, err.isEmpty() ? QStringLiteral("업로드에 실패했습니다.") : err);
                    return;
                }
                if (!publicUrl.isEmpty()) {
                    if (copyUrl)
                        QGuiApplication::clipboard()->setText(publicUrl);
                    if (openUrl)
                        QDesktopServices::openUrl(QUrl(publicUrl));
                }
                done(true, publicUrl);
            });
}

void ftpTest(const Config &config, QObject *context, FtpCallback done)
{
    runCurl({QStringLiteral("--list-only"), ftpBaseUrl(config)}, config, context,
            [done](bool ok, const QString &err, const QByteArray &) {
                done(ok, ok ? QStringLiteral("연결에 성공했습니다.")
                            : (err.isEmpty() ? QStringLiteral("연결에 실패했습니다.") : err));
            });
}

} // namespace mm::outputs
