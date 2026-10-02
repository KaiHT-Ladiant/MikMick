#include "options/HotkeyText.h"

#include <QStringList>

namespace mm::options {

HotkeyParts parseHotkey(const QString &seq)
{
    HotkeyParts parts;
    for (const QString &token : seq.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        if (token == QLatin1String("Shift"))
            parts.shift = true;
        else if (token == QLatin1String("Ctrl"))
            parts.ctrl = true;
        else if (token == QLatin1String("Alt"))
            parts.alt = true;
        else
            parts.key = token;
    }
    return parts;
}

QString buildHotkey(bool shift, bool ctrl, bool alt, const QString &key)
{
    if (key.isEmpty())
        return QString();
    QStringList parts;
    if (shift)
        parts << QStringLiteral("Shift");
    if (ctrl)
        parts << QStringLiteral("Ctrl");
    if (alt)
        parts << QStringLiteral("Alt");
    parts << key;
    return parts.join(QLatin1Char('+'));
}

QString buildHotkey(const HotkeyParts &parts)
{
    return buildHotkey(parts.shift, parts.ctrl, parts.alt, parts.key);
}

QString toPortalTrigger(const QString &seq)
{
    const HotkeyParts parts = parseHotkey(seq);
    if (parts.key.isEmpty())
        return QString();
    QStringList out;
    if (parts.shift)
        out << QStringLiteral("SHIFT");
    if (parts.ctrl)
        out << QStringLiteral("CTRL");
    if (parts.alt)
        out << QStringLiteral("ALT");
    // Portal triggers use XKB keysym names, which are lower case for letters.
    out << (parts.key.size() == 1 ? parts.key.toLower() : parts.key);
    return out.join(QLatin1Char('+'));
}

} // namespace mm::options
