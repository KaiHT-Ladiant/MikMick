#include "core/Config.h"

#include "core/Catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace mm {

namespace {

QJsonObject obj(std::initializer_list<QPair<QString, QJsonValue>> items)
{
    QJsonObject o;
    for (const auto &kv : items)
        o.insert(kv.first, kv.second);
    return o;
}

} // namespace

QString configDir()
{
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/mikmick");
}

QString picturesDir()
{
    const QString env = qEnvironmentVariable("XDG_PICTURES_DIR");
    if (!env.isEmpty())
        return env;
    const QString pics = QDir::homePath() + QStringLiteral("/Pictures");
    return QFileInfo(pics).isDir() ? pics : QDir::homePath();
}

QJsonObject Config::defaults()
{
    QJsonObject hotkeys;
    for (const auto &kv : defaultHotkeys())
        hotkeys.insert(kv.first, kv.second);

    QJsonObject d;
    d.insert(QStringLiteral("general"), obj({
        {QStringLiteral("start_mode"), QStringLiteral("editor")}, // editor | tray
        {QStringLiteral("autostart"), false},
        {QStringLiteral("autostart_mode"), QStringLiteral("tray")}, // tray | editor
        {QStringLiteral("check_updates"), true},
        {QStringLiteral("hide_start_page"), false},
    }));
    d.insert(QStringLiteral("editor"), obj({
        {QStringLiteral("hide_on_capture"), true},
        {QStringLiteral("center_image"), false},
        {QStringLiteral("auto_shape_selection"), false},
        {QStringLiteral("no_save_prompt"), false},
        {QStringLiteral("quit_on_close"), false},
        {QStringLiteral("bg_mode"), QStringLiteral("solid")}, // solid | grid
        {QStringLiteral("bg_color"), QStringLiteral("#dcdcdc")},
    }));
    d.insert(QStringLiteral("capture"), obj({
        {QStringLiteral("result"), QStringLiteral("editor")},
        {QStringLiteral("multi_monitor"), true},
        {QStringLiteral("sound"), true},
        {QStringLiteral("include_cursor"), false},
        {QStringLiteral("always_clipboard"), false},
        {QStringLiteral("show_toolbar"), true},
        {QStringLiteral("delay_ms"), 0},
        {QStringLiteral("scroll_delay_ms"), 100},
        {QStringLiteral("magnifier"), true},
        {QStringLiteral("magnifier_zoom"), 6},
        {QStringLiteral("fixed_width"), 640},
        {QStringLiteral("fixed_height"), 480},
    }));
    d.insert(QStringLiteral("filename"), obj({
        {QStringLiteral("pattern"), QStringLiteral("%c")},
        {QStringLiteral("format"), QStringLiteral("png")},
        {QStringLiteral("counter"), 0},
    }));
    d.insert(QStringLiteral("autosave"), obj({
        {QStringLiteral("enabled"), false},
        {QStringLiteral("folder"), QString()},
        {QStringLiteral("program"), QString()},
        {QStringLiteral("program_args"), QString()},
    }));
    d.insert(QStringLiteral("image"), obj({{QStringLiteral("jpeg_quality"), 100}}));
    d.insert(QStringLiteral("ftp"), obj({
        {QStringLiteral("server"), QString()},
        {QStringLiteral("port"), 21},
        {QStringLiteral("path"), QString()},
        {QStringLiteral("passive"), false},
        {QStringLiteral("user"), QStringLiteral("anonymous")},
        {QStringLiteral("password"), QString()},
        {QStringLiteral("open_url"), false},
        {QStringLiteral("copy_url"), false},
        {QStringLiteral("url"), QString()},
    }));
    d.insert(QStringLiteral("hotkeys"), hotkeys);
    d.insert(QStringLiteral("new_image"), obj({
        {QStringLiteral("preset"), QStringLiteral("clipboard")},
        {QStringLiteral("width"), 1131},
        {QStringLiteral("height"), 720},
        {QStringLiteral("bg"), QStringLiteral("#ffffff")},
    }));
    d.insert(QStringLiteral("palette"), obj({
        {QStringLiteral("custom"), QJsonArray()},
        {QStringLiteral("format"), QStringLiteral("hex")},
    }));
    d.insert(QStringLiteral("ruler"), obj({
        {QStringLiteral("unit"), QStringLiteral("px")},
        {QStringLiteral("dpi"), 96},
    }));
    d.insert(QStringLiteral("recent"), obj({
        {QStringLiteral("files"), QJsonArray()},
        {QStringLiteral("last_region"), QJsonValue()},
        {QStringLiteral("last_mode"), QJsonValue()},
    }));
    return d;
}

Config::Config(const QString &path)
    : m_path(path.isEmpty() ? configDir() + QStringLiteral("/config.json") : path)
    , m_data(defaults())
{
    load();
}

void Config::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const QJsonObject stored = doc.object();
    for (auto it = stored.begin(); it != stored.end(); ++it) {
        if (!m_data.contains(it.key()) || !it.value().isObject())
            continue;
        QJsonObject merged = m_data.value(it.key()).toObject();
        const QJsonObject values = it.value().toObject();
        for (auto v = values.begin(); v != values.end(); ++v)
            merged.insert(v.key(), v.value());
        m_data.insert(it.key(), merged);
    }
}

bool Config::save() const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonValue Config::value(const QString &section, const QString &key, const QJsonValue &fallback) const
{
    const QJsonObject s = m_data.value(section).toObject();
    auto it = s.constFind(key);
    return it == s.constEnd() ? fallback : it.value();
}

QString Config::str(const QString &section, const QString &key, const QString &fallback) const
{
    const QJsonValue v = value(section, key);
    return v.isString() ? v.toString() : fallback;
}

int Config::num(const QString &section, const QString &key, int fallback) const
{
    const QJsonValue v = value(section, key);
    return v.isDouble() ? v.toInt(qRound(v.toDouble())) : fallback;
}

double Config::real(const QString &section, const QString &key, double fallback) const
{
    const QJsonValue v = value(section, key);
    return v.isDouble() ? v.toDouble() : fallback;
}

bool Config::flag(const QString &section, const QString &key, bool fallback) const
{
    const QJsonValue v = value(section, key);
    return v.isBool() ? v.toBool() : fallback;
}

QStringList Config::list(const QString &section, const QString &key) const
{
    QStringList out;
    for (const QJsonValue &v : value(section, key).toArray()) {
        if (v.isString())
            out << v.toString();
    }
    return out;
}

void Config::set(const QString &section, const QString &key, const QJsonValue &value)
{
    QJsonObject s = m_data.value(section).toObject();
    s.insert(key, value);
    m_data.insert(section, s);
}

void Config::set(const QString &section, const QString &key, const QStringList &value)
{
    set(section, key, QJsonArray::fromStringList(value));
}

QJsonObject Config::section(const QString &section) const
{
    return m_data.value(section).toObject();
}

void Config::setSection(const QString &section, const QJsonObject &values)
{
    m_data.insert(section, values);
}

void Config::addRecentFile(const QString &file, int limit)
{
    QStringList files = list(QStringLiteral("recent"), QStringLiteral("files"));
    files.removeAll(file);
    files.prepend(file);
    while (files.size() > limit)
        files.removeLast();
    set(QStringLiteral("recent"), QStringLiteral("files"), files);
}

} // namespace mm
