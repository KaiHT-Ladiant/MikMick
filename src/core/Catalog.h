#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace mm {

struct CaptureModeInfo
{
    QString id;    // "fullscreen", "region", ...
    QString icon;  // icon name for mm::icons
    QString label; // Korean menu label
};

struct ToolInfo
{
    QString id;
    QString icon;
    QString title;
    QString description;
};

// The eight capture modes in PicPick order.
const QList<CaptureModeInfo> &captureModes();
QStringList captureModeIds();
QString captureModeLabel(const QString &id);
bool isCaptureMode(const QString &id);

// The seven graphic tools (excluding the editor itself).
const QList<ToolInfo> &graphicTools();
// Graphic tools plus "editor".
QStringList toolIds();
QString toolLabel(const QString &id);
bool isTool(const QString &id);

// Capture result targets: editor, clipboard, file, autosave, ftp, program.
const QList<QPair<QString, QString>> &resultChoices();

// Hotkey defaults keyed by capture mode / tool id (empty string = unassigned).
const QList<QPair<QString, QString>> &defaultHotkeys();

} // namespace mm
