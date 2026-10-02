#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <functional>

namespace mm {

class Config;

namespace outputs {

// Saves PNG/JPEG/BMP/GIF/WebP/TIFF/PDF depending on the extension (JPEG quality from config).
bool saveImage(const QImage &image, const QString &path, const Config &config);

struct GeneratedName
{
    QString name;
    QString ext;
};
// Builds a name from the configured pattern and increments/persists the counter.
GeneratedName nextFilename(Config &config);

QString autosaveFolder(const Config &config);
// Returns the written path, or an empty string on failure.
QString autosave(const QImage &image, Config &config);
QString tempSave(const QImage &image, Config &config);

void copyToClipboard(const QImage &image);

// Runs the configured external program; %f in its arguments is replaced by the path.
bool runProgram(const QString &path, const Config &config);
void openWithDefault(const QString &path);
bool sendEmail(const QString &path);

// FTP uses curl(1) so FTPS/SFTP URLs also work. Callbacks run on the GUI thread.
using FtpCallback = std::function<void(bool ok, const QString &message)>;
// On success the message is the public URL (when ftp.url is set) or an empty string.
void ftpUpload(const QString &path, const Config &config, QObject *context, FtpCallback done);
void ftpTest(const Config &config, QObject *context, FtpCallback done);

} // namespace outputs
} // namespace mm
