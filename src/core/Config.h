#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace mm {

// $XDG_CONFIG_HOME/mikmick
QString configDir();
// $XDG_PICTURES_DIR, ~/Pictures or ~
QString picturesDir();

// User settings persisted as JSON ($XDG_CONFIG_HOME/mikmick/config.json).
// Values are grouped by section; unknown keys in a known section are kept.
class Config
{
public:
    explicit Config(const QString &path = QString());

    static QJsonObject defaults();

    const QString &path() const { return m_path; }
    void load();
    bool save() const;

    QJsonValue value(const QString &section, const QString &key,
                     const QJsonValue &fallback = QJsonValue()) const;
    QString str(const QString &section, const QString &key, const QString &fallback = QString()) const;
    int num(const QString &section, const QString &key, int fallback = 0) const;
    double real(const QString &section, const QString &key, double fallback = 0.0) const;
    bool flag(const QString &section, const QString &key, bool fallback = false) const;
    QStringList list(const QString &section, const QString &key) const;

    void set(const QString &section, const QString &key, const QJsonValue &value);
    void set(const QString &section, const QString &key, const QStringList &value);

    QJsonObject section(const QString &section) const;
    void setSection(const QString &section, const QJsonObject &values);

    QJsonObject snapshot() const { return m_data; }
    void restore(const QJsonObject &data) { m_data = data; }

    void addRecentFile(const QString &file, int limit = 10);

private:
    QString m_path;
    QJsonObject m_data;
};

} // namespace mm
