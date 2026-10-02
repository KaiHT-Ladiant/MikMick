#pragma once

#include <QString>

namespace mm::options {

// "Shift+Ctrl+Alt+Key" as stored in the config (modifier order is not significant when parsing).
struct HotkeyParts
{
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    QString key; // "Print", "F1".."F12", "A".."Z", "0".."9"; empty = unassigned

    bool operator==(const HotkeyParts &o) const
    {
        return shift == o.shift && ctrl == o.ctrl && alt == o.alt && key == o.key;
    }
};

HotkeyParts parseHotkey(const QString &seq);
// Canonical "Shift+Ctrl+Alt+Key" order; empty when key is empty.
QString buildHotkey(bool shift, bool ctrl, bool alt, const QString &key);
QString buildHotkey(const HotkeyParts &parts);

// xdg-desktop-portal shortcut syntax ("SHIFT+CTRL+Print"); empty when unassigned.
QString toPortalTrigger(const QString &seq);

} // namespace mm::options
