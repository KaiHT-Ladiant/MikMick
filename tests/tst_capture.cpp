#include "capture/Backend.h"
#include "capture/ScrollCapture.h"
#include "tools/ColorTools.h"

#include <QRandomGenerator>
#include <QTest>

using namespace mm;

namespace {

QImage randomPage(int height = 300, int width = 20, quint32 seed = 1)
{
    QImage img(width, height, QImage::Format_ARGB32);
    QRandomGenerator rng(seed);
    for (int y = 0; y < height; ++y) {
        auto *row = reinterpret_cast<quint32 *>(img.scanLine(y));
        for (int x = 0; x < width; ++x)
            row[x] = rng.generate();
    }
    return img;
}

QImage rows(const QImage &page, int from, int count)
{
    return page.copy(0, from, page.width(), count);
}

} // namespace

class TestCapture : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findOverlapDetectsScrollDistance()
    {
        const QImage page = randomPage();
        QCOMPARE(capture::findOverlap(rows(page, 0, 100), rows(page, 30, 100)), std::optional<int>(70));
    }

    void findOverlapUnchangedReturnsNullopt()
    {
        const QImage page = randomPage();
        QVERIFY(!capture::findOverlap(rows(page, 0, 100), rows(page, 0, 100)).has_value());
    }

    void findOverlapNoMatchReturnsZero()
    {
        QCOMPARE(capture::findOverlap(randomPage(100, 20, 1), randomPage(100, 20, 2)), std::optional<int>(0));
    }

    void stitchReconstructsPage()
    {
        const QImage page = randomPage();
        QList<QImage> frames;
        for (int from : {0, 40, 80, 120, 160, 200, 200})
            frames << rows(page, from, 100);
        const QImage out = capture::stitch(frames);
        QCOMPARE(out.size(), page.size());
        QCOMPARE(out.convertToFormat(QImage::Format_ARGB32), page);
    }

    void snapshotCropUsesLogicalCoordinates()
    {
        QPixmap pm(400, 200);
        pm.fill(Qt::red);
        capture::Snapshot snap{pm, QRect(100, 50, 200, 100)};
        QCOMPARE(snap.scaleX(), 2.0);
        const QPixmap part = snap.crop(QRect(150, 60, 20, 10));
        QCOMPARE(part.size(), QSize(40, 20));
    }

    void colorFormats()
    {
        const QColor c(255, 136, 0);
        QCOMPARE(tools::formatColor(c, QStringLiteral("html")), QStringLiteral("#FF8800"));
        QCOMPARE(tools::formatColor(c, QStringLiteral("hex")), QStringLiteral("FF8800"));
        QCOMPARE(tools::formatColor(c, QStringLiteral("rgb")), QStringLiteral("rgb(255, 136, 0)"));
        QCOMPARE(tools::formatColor(c, QStringLiteral("cpp")), QStringLiteral("0x000088FF"));
        QCOMPARE(tools::formatColor(c, QStringLiteral("delphi")), QStringLiteral("$000088FF"));
    }
};

QTEST_MAIN(TestCapture)
#include "tst_capture.moc"
