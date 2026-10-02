#include "core/AppContext.h"
#include "core/Config.h"
#include "editor/Canvas.h"
#include "editor/EditorWindow.h"
#include "editor/Effects.h"
#include "editor/Items.h"

#include <QApplication>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace mm;
using namespace mm::editor;

namespace {

class StubContext : public AppContext
{
public:
    explicit StubContext(const QString &configPath)
        : m_config(configPath)
    {
    }
    Config &config() override { return m_config; }
    void capture(const QString &, int) override {}
    void openTool(const QString &) override {}
    void showOptions(QWidget *) override {}
    void shareImage(const QImage &, const QString &, QWidget *, const QString &) override {}
    void homepage() override {}
    void checkUpdates(bool, QWidget *) override {}
    bool trayAvailable() const override { return false; }
    void notify(const QString &) override {}
    void quit() override {}

private:
    Config m_config;
};

QImage solid(int w, int h, const QColor &c)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(c);
    return img;
}

void drag(CanvasView *view, const QPoint &a, const QPoint &b)
{
    QWidget *vp = view->viewport();
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, a);
    // QTest::mouseMove only moves the cursor on the offscreen platform before Qt 6.3.
    for (int i = 1; i <= 5; ++i) {
        const QPointF pos = a + (b - a) * (i / 5.0);
        QMouseEvent move(QEvent::MouseMove, pos, vp->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(vp, &move);
    }
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, b);
}

} // namespace

class TestEditor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---------------------------------------------------------------- effects
    void grayscale()
    {
        const QColor c = effects::grayscale(solid(40, 30, QColor(200, 100, 50))).pixelColor(0, 0);
        QCOMPARE(c.red(), c.green());
        QCOMPARE(c.green(), c.blue());
    }

    void invertWholeAndRegion()
    {
        const QImage img = solid(40, 30, QColor(200, 100, 50));
        QCOMPARE(effects::invert(img).pixelColor(1, 1), QColor(55, 155, 205));
        const QImage part = effects::invert(img, QRect(0, 0, 10, 10));
        QCOMPARE(part.pixelColor(1, 1), QColor(55, 155, 205));
        QCOMPARE(part.pixelColor(20, 20), QColor(200, 100, 50));
    }

    void blurKeepsUniformColor()
    {
        const QColor c = effects::blur(solid(40, 30, QColor(200, 100, 50)), 3).pixelColor(20, 15);
        QVERIFY(qAbs(c.red() - 200) <= 1 && qAbs(c.green() - 100) <= 1);
    }

    void mosaicAveragesBlocks()
    {
        QImage img = solid(4, 4, Qt::black);
        img.setPixelColor(0, 0, Qt::white);
        const QImage out = effects::mosaic(img, 2);
        QVERIFY(qAbs(out.pixelColor(1, 1).red() - 64) <= 1);
        QCOMPARE(out.pixelColor(3, 3).red(), 0);
    }

    void rotateAndFlip()
    {
        const QImage img = solid(40, 30, Qt::red);
        QCOMPARE(effects::rotate(img, 90).width(), 30);
        QCOMPARE(effects::flip(img, true).size(), img.size());
    }

    void borderGrowsImage()
    {
        const QImage img = solid(40, 30, QColor(200, 100, 50));
        const QImage out = effects::border(img, 5, Qt::black);
        QCOMPARE(out.size(), QSize(50, 40));
        QCOMPARE(out.pixelColor(0, 0), QColor(Qt::black));
        QCOMPARE(effects::border(img, 5, Qt::black, true).size(), img.size());
    }

    void canvasSizeAnchor()
    {
        const QImage out = effects::canvasSize(solid(40, 30, QColor(200, 100, 50)), 60, 50,
                                               QStringLiteral("top-left"), Qt::white);
        QCOMPARE(out.pixelColor(0, 0), QColor(200, 100, 50));
        QCOMPARE(out.pixelColor(59, 49), QColor(Qt::white));
    }

    void dropShadowGrows()
    {
        const QImage img = solid(40, 30, Qt::red);
        const QImage out = effects::dropShadow(img, 4, 2);
        QVERIFY(out.width() > img.width() && out.height() > img.height());
    }

    void hueSaturationIdentity()
    {
        const QColor c = effects::hueSaturation(solid(40, 30, QColor(200, 100, 50)), 0, 0, 0).pixelColor(3, 3);
        QVERIFY(qAbs(c.red() - 200) <= 1 && qAbs(c.green() - 100) <= 1 && qAbs(c.blue() - 50) <= 1);
    }

    void brightness()
    {
        QCOMPARE(effects::brightnessContrast(solid(40, 30, QColor(200, 100, 50)), 100, 0).pixelColor(0, 0).red(), 255);
    }

    void floodFillStopsAtBorder()
    {
        QImage img = solid(10, 10, Qt::white);
        for (int y = 0; y < 10; ++y)
            img.setPixelColor(5, y, Qt::black);
        const QImage out = effects::floodFill(img, 1, 1, Qt::red, 0);
        QCOMPARE(out.pixelColor(0, 9), QColor(Qt::red));
        QCOMPARE(out.pixelColor(5, 5), QColor(Qt::black));
        QCOMPARE(out.pixelColor(8, 8), QColor(Qt::white));
        int red = 0;
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x)
                red += out.pixelColor(x, y) == QColor(Qt::red);
        }
        QCOMPARE(red, 50);
    }

    void watermarkChangesPixels()
    {
        const QImage out = effects::watermark(solid(200, 100, Qt::black), QStringLiteral("MikMick"), Qt::white, 1.0,
                                              QStringLiteral("center"), 30);
        bool changed = false;
        for (int y = 0; y < out.height() && !changed; ++y) {
            for (int x = 0; x < out.width() && !changed; ++x)
                changed = qRed(out.pixel(x, y)) > 0;
        }
        QVERIFY(changed);
    }

    // ---------------------------------------------------------------- document
    void addItemUndoRedo()
    {
        Document doc(solid(100, 80, Qt::white));
        auto *item = new ShapeItem(QStringLiteral("rect"), QPointF(10, 10), QPointF(50, 40), Qt::red, 3);
        doc.addItem(item);
        QVERIFY(doc.objects().contains(item));
        doc.undoStack()->undo();
        QVERIFY(!doc.objects().contains(item));
        doc.undoStack()->redo();
        QVERIFY(doc.objects().contains(item));
    }

    void flattenIncludesObjects()
    {
        Document doc(solid(100, 80, Qt::white));
        doc.addItem(new ShapeItem(QStringLiteral("rect"), QPointF(10, 10), QPointF(50, 40), Qt::red, 4, Qt::red));
        const QImage flat = doc.flatten();
        QCOMPARE(flat.pixelColor(30, 25).red(), 255);
        QCOMPARE(flat.pixelColor(30, 25).green(), 0);
        QCOMPARE(flat.pixelColor(90, 70), QColor(Qt::white));
    }

    void cropSelectionMergesAndIsUndoable()
    {
        Document doc(solid(100, 80, Qt::white));
        doc.addItem(new ShapeItem(QStringLiteral("ellipse"), QPointF(0, 0), QPointF(20, 20), Qt::blue, 2));
        doc.setSelection(QRect(10, 10, 30, 20));
        QVERIFY(doc.cropSelection());
        QCOMPARE(doc.size(), QSize(30, 20));
        QVERIFY(doc.objects().isEmpty());
        doc.undoStack()->undo();
        QCOMPARE(doc.size(), QSize(100, 80));
        QCOMPARE(doc.objects().size(), 1);
    }

    void modifiedFlag()
    {
        Document doc(solid(100, 80, Qt::white));
        QVERIFY(doc.isModified());
        doc.setPath(QStringLiteral("/tmp/x.png"));
        doc.undoStack()->setClean();
        QVERIFY(!doc.isModified());
        doc.applyRaster([](const QImage &img) { return effects::flip(img, true); }, QStringLiteral("flip"));
        QVERIFY(doc.isModified());
    }

    // ---------------------------------------------------------------- window
    void drawSelectEffectCropSave()
    {
        QTemporaryDir dir;
        StubContext ctx(dir.filePath(QStringLiteral("config.json")));
        ctx.config().set(QStringLiteral("editor"), QStringLiteral("no_save_prompt"), true);
        EditorWindow ed(ctx);
        ed.resize(1200, 800);
        ed.show();
        Document *doc = ed.addImage(solid(300, 200, QColor(10, 120, 200)), QString(), QStringLiteral("t"));
        CanvasView *view = ed.currentView();
        view->setZoom(1.0);
        QApplication::processEvents();

        ed.setTool(QStringLiteral("shape"));
        drag(view, view->mapFromScene(20, 20), view->mapFromScene(120, 90));
        QCOMPARE(doc->objects().size(), 1);

        ed.setTool(QStringLiteral("select"));
        drag(view, view->mapFromScene(10, 10), view->mapFromScene(160, 110));
        QVERIFY(doc->hasSelection());
        QVERIFY(doc->selection().width() >= 140);

        ed.applyEffect(QStringLiteral("무채화"),
                       [](const QImage &img, const QRect &r) { return effects::grayscale(img, r); });
        const QColor c = doc->image().pixelColor(150, 100);
        QCOMPARE(c.red(), c.green());
        QCOMPARE(c.green(), c.blue());
        QVERIFY(doc->objects().isEmpty());

        ed.crop();
        QVERIFY(doc->size().width() < 300);

        const QString path = dir.filePath(QStringLiteral("out.png"));
        QVERIFY(ed.saveTo(doc, path));
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(!doc->isModified());

        ed.undo();
        QCOMPARE(doc->size().width(), 300);

        QVERIFY(ed.openPath(path));
        QCOMPARE(ed.tabs()->count(), 2);
        QVERIFY(ed.closeAll());
    }

    void stampAndDeleteTools()
    {
        QTemporaryDir dir;
        StubContext ctx(dir.filePath(QStringLiteral("config.json")));
        ctx.config().set(QStringLiteral("editor"), QStringLiteral("no_save_prompt"), true);
        EditorWindow ed(ctx);
        ed.resize(1200, 800);
        ed.show();
        Document *doc = ed.addImage(solid(300, 200, QColor(10, 120, 200)));
        CanvasView *view = ed.currentView();
        view->setZoom(1.0);
        QApplication::processEvents();

        ed.setStampKind(QStringLiteral("number"));
        for (int x : {30, 60, 90})
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(x, 40));
        QList<int> numbers;
        for (QGraphicsItem *it : doc->objects()) {
            if (auto *stamp = dynamic_cast<StampItem *>(it))
                numbers << stamp->number();
        }
        std::sort(numbers.begin(), numbers.end());
        QCOMPARE(numbers, QList<int>({1, 2, 3}));

        ed.setTool(QStringLiteral("move"));
        doc->scene()->clearSelection();
        doc->objects().first()->setSelected(true);
        ed.deleteSelection();
        QCOMPARE(doc->objects().size(), 2);
        ed.undo();
        QCOMPARE(doc->objects().size(), 3);
        QVERIFY(ed.closeAll());
    }

    void backstageHidesStatusBar()
    {
        QTemporaryDir dir;
        StubContext ctx(dir.filePath(QStringLiteral("config.json")));
        EditorWindow ed(ctx);
        ed.show();
        ed.showBackstage(QStringLiteral("start"));
        QVERIFY(ed.isBackstageVisible());
        ed.addImage(solid(50, 50, Qt::white));
        ed.hideBackstage();
        QVERIFY(!ed.isBackstageVisible());
        QVERIFY(ed.hasDocument());
        ctx.config().set(QStringLiteral("editor"), QStringLiteral("no_save_prompt"), true);
        QVERIFY(ed.closeAll());
    }
};

QTEST_MAIN(TestEditor)
#include "tst_editor.moc"
