#pragma once

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>

namespace mm {

// (extension, file dialog filter) for every format the editor can write.
const QList<QPair<QString, QString>> &imageFormats();
// "PNG (*.png);;JPEG (*.jpg);;..." for QFileDialog.
QString imageSaveFilter();
QString imageOpenFilter();

// Expands %y %m %d %h %n %s %c %u %w %t %% in an auto-save file name pattern.
QString expandPattern(const QString &pattern, int counter, const QDateTime &now = QDateTime::currentDateTime());

// folder/stem.ext, or folder/stem (1).ext ... when the file already exists.
QString uniquePath(const QString &folder, const QString &stem, const QString &ext);

// Lower-case extension without the dot.
QString extOf(const QString &path);

} // namespace mm
