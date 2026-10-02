#include "core/Catalog.h"
#include "core/Config.h"
#include "core/Filename.h"
#include "core/System.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace mm;

class TestCore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterIsZeroPadded()
    {
        QCOMPARE(expandPattern(QStringLiteral("%c"), 0), QStringLiteral("000"));
        QCOMPARE(expandPattern(QStringLiteral("shot_%c"), 42), QStringLiteral("shot_042"));
    }

    void dateTokens()
    {
        const QDateTime now(QDate(2026, 10, 2), QTime(9, 41, 5));
        QCOMPARE(expandPattern(QStringLiteral("%y-%m-%d_%h%n%s"), 1, now), QStringLiteral("2026-10-02_094105"));
        QCOMPARE(expandPattern(QStringLiteral("%t"), 1, now), QString::number(now.toSecsSinceEpoch()));
    }

    void unknownTokenAndLiteralPercent()
    {
        QCOMPARE(expandPattern(QStringLiteral("a%qb%%"), 1), QStringLiteral("a%qb%"));
    }

    void slashesAreSanitized()
    {
        QVERIFY(!expandPattern(QStringLiteral("a/b_%c"), 3).contains(QLatin1Char('/')));
    }

    void emptyPatternFallsBackToCounter()
    {
        QCOMPARE(expandPattern(QString(), 7), QStringLiteral("007"));
    }

    void uniquePathAddsSuffix()
    {
        QTemporaryDir dir;
        const QString first = uniquePath(dir.path(), QStringLiteral("img"), QStringLiteral("png"));
        QFile f(first);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        const QString second = uniquePath(dir.path(), QStringLiteral("img"), QStringLiteral("png"));
        QVERIFY(second.endsWith(QStringLiteral("/img (1).png")));
    }

    void extensionIsLowerCase()
    {
        QCOMPARE(extOf(QStringLiteral("/tmp/A.PNG")), QStringLiteral("png"));
        QCOMPARE(extOf(QStringLiteral("noext")), QString());
    }

    void configDefaults()
    {
        QTemporaryDir dir;
        Config config(dir.filePath(QStringLiteral("config.json")));
        QCOMPARE(config.str(QStringLiteral("capture"), QStringLiteral("result")), QStringLiteral("editor"));
        QCOMPARE(config.str(QStringLiteral("hotkeys"), QStringLiteral("region")), QStringLiteral("Shift+Print"));
        QCOMPARE(config.str(QStringLiteral("filename"), QStringLiteral("pattern")), QStringLiteral("%c"));
        QCOMPARE(config.num(QStringLiteral("capture"), QStringLiteral("magnifier_zoom")), 6);
    }

    void configRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/config.json"));
        Config config(path);
        config.set(QStringLiteral("capture"), QStringLiteral("delay_ms"), 1500);
        config.addRecentFile(QStringLiteral("/tmp/a.png"));
        config.addRecentFile(QStringLiteral("/tmp/b.png"));
        config.addRecentFile(QStringLiteral("/tmp/a.png"));
        QVERIFY(config.save());

        Config again(path);
        QCOMPARE(again.num(QStringLiteral("capture"), QStringLiteral("delay_ms")), 1500);
        QCOMPARE(again.list(QStringLiteral("recent"), QStringLiteral("files")),
                 QStringList({QStringLiteral("/tmp/a.png"), QStringLiteral("/tmp/b.png")}));
    }

    void unknownSectionsIgnoredAndMissingKeysDefault()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"bogus": {"x": 1}, "capture": {"sound": false}})");
        f.close();
        Config loaded(path);
        QVERIFY(!loaded.snapshot().contains(QStringLiteral("bogus")));
        QCOMPARE(loaded.flag(QStringLiteral("capture"), QStringLiteral("sound"), true), false);
        QCOMPARE(loaded.flag(QStringLiteral("capture"), QStringLiteral("magnifier"), false), true);
    }

    void corruptFileUsesDefaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{not json");
        f.close();
        QCOMPARE(Config(path).str(QStringLiteral("general"), QStringLiteral("start_mode")), QStringLiteral("editor"));
    }

    void versionCompare()
    {
        QCOMPARE(sys::parseVersion(QStringLiteral("v1.2.10")), QList<int>({1, 2, 10}));
        QVERIFY(sys::isNewer(QStringLiteral("v0.2.0"), QStringLiteral("0.1.9")));
        QVERIFY(sys::isNewer(QStringLiteral("0.2.1"), QStringLiteral("0.2")));
        QVERIFY(!sys::isNewer(QStringLiteral("0.1.0"), QStringLiteral("0.1.0")));
        QVERIFY(!sys::isNewer(QStringLiteral("0.1"), QStringLiteral("0.1.0")));
    }

    void catalogIsConsistent()
    {
        QCOMPARE(captureModes().size(), 8);
        QCOMPARE(graphicTools().size(), 7);
        QCOMPARE(toolIds().size(), 8);
        QVERIFY(isCaptureMode(QStringLiteral("repeat_last")));
        QVERIFY(isTool(QStringLiteral("editor")));
        QCOMPARE(captureModeLabel(QStringLiteral("region")), QStringLiteral("영역을 지정하여 캡처"));
        for (const auto &kv : defaultHotkeys())
            QVERIFY(isCaptureMode(kv.first) || isTool(kv.first));
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
