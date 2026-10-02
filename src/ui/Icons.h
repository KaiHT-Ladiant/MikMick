#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace mm::icons {

// Vector icons painted with QPainter in a 64x64 design space (no icon theme needed).
// Unknown names render a neutral placeholder square.
QIcon icon(const QString &name);
QPixmap render(const QString &name, int size = 64);
bool has(const QString &name);
QStringList names();

} // namespace mm::icons
