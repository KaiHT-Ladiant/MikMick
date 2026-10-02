#include "core/Config.h"
#include "options/HotkeyText.h"
#include "options/OptionsDialog.h"

#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using namespace mm;
using namespace mm::options;

class TestOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hotkeyParseBuildRoundTrip()
    {
        const HotkeyParts parts = parseHotkey(QStringLiteral("Shift+Ctrl+Print"));
        QVERIFY(parts.shift);
        QVERIFY(parts.ctrl);
        QVERIFY(!parts.alt);
        QCOMPARE(parts.key, QStringLiteral("Print"));
        QCOMPARE(buildHotkey(true, true, false, QStringLiteral("Print")), QStringLiteral("Shift+Ctrl+Print"));
        QCOMPARE(buildHotkey(parts), QStringLiteral("Shift+Ctrl+Print"));
        QCOMPARE(buildHotkey(true, false, false, QString()), QString());
    }

    void portalTriggerSyntax()
    {
        QCOMPARE(toPortalTrigger(QStringLiteral("Shift+Ctrl+Print")), QStringLiteral("SHIFT+CTRL+Print"));
        QCOMPARE(toPortalTrigger(QStringLiteral("Alt+A")), QStringLiteral("ALT+a"));
    }

    void dialogPagesAndAccept()
    {
        QTemporaryDir dir;
        Config config(dir.filePath(QStringLiteral("config.json")));
        OptionsDialog dlg(config);
        for (int page = OptionsDialog::General; page <= OptionsDialog::Hotkeys; ++page) {
            dlg.setCurrentPage(page);
            QCOMPARE(dlg.currentPage(), page);
        }
        QPushButton *ok = nullptr;
        for (QPushButton *b : dlg.findChildren<QPushButton *>()) {
            if (b->text().startsWith(QStringLiteral("확인")))
                ok = b;
        }
        QVERIFY(ok);
        ok->click();
        QCOMPARE(dlg.result(), int(QDialog::Accepted));
        QCOMPARE(Config(config.path()).str(QStringLiteral("hotkeys"), QStringLiteral("region")),
                 QStringLiteral("Shift+Print"));
    }
};

QTEST_MAIN(TestOptions)
#include "tst_options.moc"
