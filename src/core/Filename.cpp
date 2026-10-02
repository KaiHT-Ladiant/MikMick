#include "core/Filename.h"

#include <QDir>
#include <QFileInfo>
#include <QHostInfo>

namespace mm {

const QList<QPair<QString, QString>> &imageFormats()
{
    static const QList<QPair<QString, QString>> formats = {
        {QStringLiteral("png"), QStringLiteral("Portable Network Graphics (*.png)")},
        {QStringLiteral("jpg"), QStringLiteral("JPEG (*.jpg)")},
        {QStringLiteral("bmp"), QStringLiteral("Windows Bitmap (*.bmp)")},
        {QStringLiteral("gif"), QStringLiteral("Graphics Interchange Format (*.gif)")},
        {QStringLiteral("webp"), QStringLiteral("WebP (*.webp)")},
        {QStringLiteral("tiff"), QStringLiteral("Tagged Image File Format (*.tiff)")},
        {QStringLiteral("pdf"), QStringLiteral("Portable Document Format (*.pdf)")},
    };
    return formats;
}

QString imageSaveFilter()
{
    QStringList parts;
    for (const auto &f : imageFormats())
        parts << f.second;
    return parts.join(QStringLiteral(";;"));
}

QString imageOpenFilter()
{
    return QStringLiteral("이미지 파일 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;모든 파일 (*)");
}

static QString userName()
{
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = qEnvironmentVariable("LOGNAME");
    return user.isEmpty() ? QStringLiteral("user") : user;
}

QString expandPattern(const QString &pattern, int counter, const QDateTime &now)
{
    const QString src = pattern.isEmpty() ? QStringLiteral("%c") : pattern;
    const QString counterText = QStringLiteral("%1").arg(counter, 3, 10, QLatin1Char('0'));
    QString out;
    out.reserve(src.size() + 16);
    for (int i = 0; i < src.size(); ++i) {
        const QChar ch = src.at(i);
        if (ch != QLatin1Char('%') || i + 1 >= src.size()) {
            out += ch;
            continue;
        }
        const QChar code = src.at(++i);
        switch (code.unicode()) {
        case 'y': out += now.toString(QStringLiteral("yyyy")); break;
        case 'm': out += now.toString(QStringLiteral("MM")); break;
        case 'd': out += now.toString(QStringLiteral("dd")); break;
        case 'h': out += now.toString(QStringLiteral("HH")); break;
        case 'n': out += now.toString(QStringLiteral("mm")); break;
        case 's': out += now.toString(QStringLiteral("ss")); break;
        case 'c': out += counterText; break;
        case 'u': out += userName(); break;
        case 'w': out += QHostInfo::localHostName(); break;
        case 't': out += QString::number(now.toSecsSinceEpoch()); break;
        case '%': out += QLatin1Char('%'); break;
        default:
            out += QLatin1Char('%');
            out += code;
        }
    }
    out.replace(QLatin1Char('/'), QLatin1Char('_'));
    out.remove(QChar(0));
    out = out.trimmed();
    return out.isEmpty() ? counterText : out;
}

QString uniquePath(const QString &folder, const QString &stem, const QString &ext)
{
    const QDir dir(folder);
    QString candidate = dir.filePath(stem + QLatin1Char('.') + ext);
    for (int index = 1; QFileInfo::exists(candidate); ++index)
        candidate = dir.filePath(QStringLiteral("%1 (%2).%3").arg(stem).arg(index).arg(ext));
    return candidate;
}

QString extOf(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

} // namespace mm
