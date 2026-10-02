// Renders the README screenshots offscreen:
//   QT_QPA_PLATFORM=offscreen ./build/tests/mikmick-screenshots docs/screenshots

#include "app/Controller.h"
#include "capture/CaptureOverlay.h"
#include "core/Config.h"
#include "editor/Canvas.h"
#include "editor/EditorWindow.h"
#include "editor/Items.h"
#include "options/OptionsDialog.h"
#include "tools/ColorTools.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QLinearGradient>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

using namespace mm;

namespace {

QString g_out;

QImage sampleImage(int w = 900, int h = 520)
{
    QImage img(w, h, QImage::Format_ARGB32);
    QPainter p(&img);
    QLinearGradient grad(0, 0, w, h);
    grad.setColorAt(0, QColor(QStringLiteral("#2c3e50")));
    grad.setColorAt(1, QColor(QStringLiteral("#4ca1af")));
    p.fillRect(img.rect(), grad);
    p.fillRect(QRect(60, 50, w - 120, h - 100), QColor(QStringLiteral("#fdfdfd")));
    p.fillRect(QRect(60, 50, w - 120, 34), QColor(QStringLiteral("#e8e8e8")));
    p.setPen(QColor(QStringLiteral("#333")));
    QFont f;
    f.setPixelSize(22);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(90, 110, 600, 40), 0, QStringLiteral("MikMick - Linux Screen Capture"));
    for (int i = 0; i < 8; ++i)
        p.fillRect(QRect(90, 170 + i * 30, 300 + (i * 37) % 260, 12), QColor(QStringLiteral("#c9d3dd")));
    p.fillRect(QRect(560, 170, 220, 220), QColor(QStringLiteral("#ffd166")));
    return img;
}

void save(QWidget *widget, const QString &name)
{
    QApplication::processEvents();
    const QPixmap pm = widget->grab();
    pm.save(QDir(g_out).filePath(name + QStringLiteral(".png")));
    std::printf("saved %s %dx%d\n", qPrintable(name), pm.width(), pm.height());
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? "offscreen" : qgetenv("QT_QPA_PLATFORM"));
    QTemporaryDir configHome;
    qputenv("XDG_CONFIG_HOME", configHome.path().toLocal8Bit());
    QApplication app(argc, argv);
    QApplication::setFont(QFont(QStringLiteral("Noto Sans CJK KR"), 9));
    theme::applyLightPalette();
    g_out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("docs/screenshots");
    QDir().mkpath(g_out);

    Config config;
    config.set(QStringLiteral("general"), QStringLiteral("check_updates"), false);
    Controller ctl(config);
    editor::EditorWindow *ed = ctl.editor();
    ed->resize(1240, 700);
    ed->show();
    ed->showBackstage(QStringLiteral("start"));
    save(ed, QStringLiteral("start"));
    ed->showBackstage(QStringLiteral("new"));
    save(ed, QStringLiteral("new"));
    ed->showBackstage(QStringLiteral("info"));
    save(ed, QStringLiteral("info"));
    ed->hideBackstage();
    save(ed, QStringLiteral("editor_empty"));

    editor::Document *doc = ed->addImage(sampleImage(), QString(), QStringLiteral("캡처 이미지"));
    const QColor red(QStringLiteral("#e81123"));
    doc->addItem(new editor::ShapeItem(QStringLiteral("rect"), QPointF(550, 160), QPointF(790, 400), red, 4));
    doc->addItem(new editor::ShapeItem(QStringLiteral("arrow"), QPointF(380, 420), QPointF(540, 330), red, 5));
    auto *s1 = new editor::StampItem(QStringLiteral("number"), 1, red, 30);
    s1->setPos(540, 150);
    doc->addItem(s1);
    auto *s2 = new editor::StampItem(QStringLiteral("number"), 2, QColor(QStringLiteral("#0078d7")), 30);
    s2->setPos(80, 115);
    doc->addItem(s2);
    auto *text = new editor::TextItem(QStringLiteral("여기를 확인하세요!"), QFont(QStringLiteral("Sans"), 18), red);
    text->setPos(250, 425);
    doc->addItem(text);
    auto *stroke = new editor::StrokeItem(QPointF(90, 300), QColor(QStringLiteral("#ffe600")), 18, true);
    for (int x = 95; x < 400; x += 5)
        stroke->addPoint(QPointF(x, 300));
    doc->addItem(stroke);
    ed->currentView()->setZoom(1.0);
    ed->setTool(QStringLiteral("shape"));
    save(ed, QStringLiteral("editor"));

    {
        options::OptionsDialog dlg(config);
        dlg.show();
        const QStringList pages = {QStringLiteral("general"), QStringLiteral("editor"), QStringLiteral("capture"),
                                   QStringLiteral("filename"), QStringLiteral("autosave"), QStringLiteral("image"),
                                   QStringLiteral("ftp"), QStringLiteral("hotkeys")};
        for (int i = 0; i < pages.size(); ++i) {
            dlg.setCurrentPage(i);
            save(&dlg, QStringLiteral("options_") + pages.at(i));
        }
    }

    {
        const capture::Snapshot snap{QPixmap::fromImage(sampleImage(1280, 720)), QRect(0, 0, 1280, 720)};
        auto *overlay = new capture::CaptureOverlay(snap, QStringLiteral("region"), true, 6);
        overlay->setGeometry(snap.geometry);
        overlay->show();
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(520, 140));
        for (int i = 1; i <= 6; ++i)
            QTest::mouseMove(overlay, QPoint(520 + 50 * i, 140 + 260 * i / 6));
        save(overlay, QStringLiteral("capture_region"));
        overlay->close();
    }

    {
        tools::ColorToolWindow cw(config, true);
        cw.setColor(QColor(QStringLiteral("#4ca1af")));
        cw.show();
        save(&cw, QStringLiteral("color_palette"));
    }

    for (editor::CanvasView *v : ed->views()) {
        v->document()->setPath(QStringLiteral("/tmp/x.png"));
        v->document()->undoStack()->setClean();
    }
    config.set(QStringLiteral("editor"), QStringLiteral("no_save_prompt"), true);
    ed->closeAll();
    return 0;
}
