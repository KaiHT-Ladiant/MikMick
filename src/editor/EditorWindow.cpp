#include "editor/EditorWindow.h"

#include "Version.h"
#include "core/AppContext.h"
#include "core/Catalog.h"
#include "core/Config.h"
#include "core/Filename.h"
#include "core/Outputs.h"
#include "editor/Backstage.h"
#include "editor/Dialogs.h"
#include "editor/Effects.h"
#include "editor/Items.h"
#include "editor/Ribbon.h"
#include "ui/Icons.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace mm::editor {

namespace {

QString appName()
{
    return QString::fromUtf8(kAppNameKo);
}

QBrush gridBrush()
{
    QPixmap pm(16, 16);
    pm.fill(QColor(QStringLiteral("#ffffff")));
    QPainter p(&pm);
    p.fillRect(0, 0, 8, 8, QColor(QStringLiteral("#cccccc")));
    p.fillRect(8, 8, 8, 8, QColor(QStringLiteral("#cccccc")));
    p.end();
    return QBrush(pm);
}

int roundHalfEven(double v)
{
    return int(std::nearbyint(v));
}

const QString kEditor = QStringLiteral("editor");

} // namespace

EditorWindow::EditorWindow(AppContext &ctx, QWidget *parent)
    : QMainWindow(parent)
    , m_ctx(ctx)
    , m_config(ctx.config())
{
    setWindowTitle(appName());
    setWindowIcon(icons::icon(QStringLiteral("logo")));
    resize(1100, 720);
    setAcceptDrops(true);

    buildActions();
    m_stack = new QStackedWidget();
    setCentralWidget(m_stack);

    auto *editor = new QWidget();
    auto *lay = new QVBoxLayout(editor);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    m_ribbon = new Ribbon();
    buildRibbon();
    lay->addWidget(m_ribbon);
    m_tabs = new QTabWidget();
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int i) { closeDocument(i); });
    connect(m_tabs, &QTabWidget::currentChanged, this, &EditorWindow::onTabChanged);
    m_empty = new QWidget();
    m_empty->setAutoFillBackground(true);
    m_canvasStack = new QStackedWidget();
    m_canvasStack->addWidget(m_empty);
    m_canvasStack->addWidget(m_tabs);
    lay->addWidget(m_canvasStack, 1);
    m_stack->addWidget(editor);

    const QString ni = QStringLiteral("new_image");
    m_backstage = new Backstage(m_config.flag(QStringLiteral("general"), QStringLiteral("hide_start_page"), false),
                                m_config.str(ni, QStringLiteral("preset"), QStringLiteral("clipboard")),
                                QSize(m_config.num(ni, QStringLiteral("width"), 1131),
                                      m_config.num(ni, QStringLiteral("height"), 720)),
                                QColor(m_config.str(ni, QStringLiteral("bg"), QStringLiteral("#ffffff"))));
    connect(m_backstage, &Backstage::back, this, &EditorWindow::hideBackstage);
    connect(m_backstage, &Backstage::command, this, &EditorWindow::onBackstage);
    connect(m_backstage->startPage(), &StartPage::hideChanged, this, [this](bool hide) {
        m_config.set(QStringLiteral("general"), QStringLiteral("hide_start_page"), hide);
        m_config.save();
    });
    m_stack->addWidget(m_backstage);

    buildStatus();
    applySettings();
    updateUi();
}

// ---------------------------------------------------------------- construction

EditorWindow::MenuEntry EditorWindow::separator()
{
    MenuEntry e;
    e.separator = true;
    return e;
}

QAction *EditorWindow::act(const QString &text, const QString &iconName, std::function<void()> slot,
                           const QKeySequence &shortcut)
{
    auto *a = new QAction(text, this);
    if (!iconName.isEmpty())
        a->setIcon(icons::icon(iconName));
    if (!shortcut.isEmpty()) {
        a->setShortcut(shortcut);
        a->setShortcutContext(Qt::WindowShortcut);
    }
    connect(a, &QAction::triggered, this, [slot = std::move(slot)] { slot(); });
    addAction(a);
    return a;
}

QMenu *EditorWindow::menu(const QList<MenuEntry> &items)
{
    auto *m = new QMenu(this);
    for (const MenuEntry &item : items) {
        if (item.separator) {
            m->addSeparator();
            continue;
        }
        QAction *a = item.icon.isEmpty() ? m->addAction(item.text) : m->addAction(icons::icon(item.icon), item.text);
        const auto slot = item.slot;
        connect(a, &QAction::triggered, this, [slot] { slot(); });
    }
    return m;
}

void EditorWindow::buildActions()
{
    m_aNew = act(QStringLiteral("새로 만들기"), QStringLiteral("new"), [this] { showBackstage(QStringLiteral("new")); },
                 QKeySequence(QKeySequence::New));
    m_aOpen = act(QStringLiteral("열기"), QStringLiteral("open"), [this] { openFiles(); }, QKeySequence(QKeySequence::Open));
    m_aSave = act(QStringLiteral("저장"), QStringLiteral("save"), [this] { save(); }, QKeySequence(QKeySequence::Save));
    m_aSaveAs = act(QStringLiteral("다른 이름으로 저장"), QStringLiteral("save_as"), [this] { saveAs(); },
                    QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    m_aPrint = act(QStringLiteral("인쇄"), QStringLiteral("print"), [this] { printImage(); },
                   QKeySequence(QKeySequence::Print));
    m_aUndo = act(QStringLiteral("실행 취소"), QStringLiteral("undo"), [this] { undo(); }, QKeySequence(QKeySequence::Undo));
    m_aRedo = act(QStringLiteral("다시 실행"), QStringLiteral("redo"), [this] { redo(); }, QKeySequence(QStringLiteral("Ctrl+Y")));
    m_aPaste = act(QStringLiteral("붙여넣기"), QStringLiteral("paste"), [this] { paste(); },
                   QKeySequence(QKeySequence::Paste));
    m_aPasteNew = act(QStringLiteral("새 이미지로 붙여넣기"), QStringLiteral("new"), [this] { pasteAsNew(); },
                      QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    m_aCut = act(QStringLiteral("잘라내기"), QStringLiteral("cut"), [this] { cut(); }, QKeySequence(QKeySequence::Cut));
    m_aCopy = act(QStringLiteral("복사"), QStringLiteral("copy"), [this] { copy(); }, QKeySequence(QKeySequence::Copy));
    m_aDelete = act(QStringLiteral("삭제"), QString(), [this] { deleteSelection(); }, QKeySequence(QKeySequence::Delete));
    m_aSelectAll = act(QStringLiteral("모두 선택"), QString(), [this] { selectAll(); },
                       QKeySequence(QKeySequence::SelectAll));
    m_aDeselect = act(QStringLiteral("선택 해제"), QString(), [this] { deselect(); }, QKeySequence(QStringLiteral("Ctrl+D")));
    m_aCrop = act(QStringLiteral("자르기"), QStringLiteral("crop"), [this] { crop(); },
                  QKeySequence(QStringLiteral("Ctrl+Shift+X")));
    m_aZoomIn = act(QStringLiteral("확대"), QStringLiteral("zoom_in"), [this] { zoomBy(1.25); },
                    QKeySequence(QKeySequence::ZoomIn));
    m_aZoomOut = act(QStringLiteral("축소"), QStringLiteral("zoom_out"), [this] { zoomBy(0.8); },
                     QKeySequence(QKeySequence::ZoomOut));
    m_aZoom100 = act(QStringLiteral("100%"), QStringLiteral("zoom_100"), [this] { setZoom(1.0); },
                     QKeySequence(QStringLiteral("Ctrl+0")));
    m_aZoomFit = act(QStringLiteral("화면에 맞추기"), QStringLiteral("zoom_fit"), [this] { fitToWindow(); },
                     QKeySequence(QStringLiteral("Ctrl+9")));
    m_aMerge = act(QStringLiteral("개체 병합"), QStringLiteral("thumbnail"), [this] { mergeObjects(); },
                   QKeySequence(QStringLiteral("Ctrl+M")));
    m_aClose = act(QStringLiteral("닫기"), QStringLiteral("close"), [this] { closeDocument(m_tabs->currentIndex()); },
                   QKeySequence(QStringLiteral("Ctrl+W")));
    m_docActions = {m_aSave,      m_aSaveAs,   m_aPrint, m_aCut,     m_aCopy,    m_aDelete,   m_aSelectAll, m_aDeselect,
                    m_aCrop,      m_aZoomIn,   m_aZoomOut, m_aZoom100, m_aZoomFit, m_aMerge, m_aClose};
}

void EditorWindow::buildRibbon()
{
    Ribbon *rb = m_ribbon;
    for (QAction *a : {m_aNew, m_aOpen, m_aSave, m_aPrint, m_aUndo, m_aRedo})
        rb->addQuick(a);
    auto *heart = new RibbonButton(QString(), QStringLiteral("heart"), false);
    heart->setToolTip(QStringLiteral("믹믹 GitHub 저장소"));
    connect(heart, &QToolButton::clicked, this, [this] { m_ctx.homepage(); });
    rb->extraLayout()->addWidget(heart);

    // Home ----------------------------------------------------------------
    auto *home = new RibbonPage();
    RibbonGroup *g = home->addGroup(new RibbonGroup(QStringLiteral("클립보드")));
    auto *pasteBtn = new RibbonButton(QStringLiteral("붙여넣기"), QStringLiteral("paste"), true,
                                      menu({
                                          {QStringLiteral("붙여넣기"), QStringLiteral("paste"), [this] { paste(); }},
                                          {QStringLiteral("새 이미지로 붙여넣기"), QStringLiteral("new"), [this] { pasteAsNew(); }},
                                      }));
    g->add(pasteBtn);
    auto *btnCut = new RibbonButton(QStringLiteral("잘라내기"), QStringLiteral("cut"), false);
    connect(btnCut, &QToolButton::clicked, this, [this] { cut(); });
    auto *btnCopy = new RibbonButton(QStringLiteral("복사"), QStringLiteral("copy"), false);
    connect(btnCopy, &QToolButton::clicked, this, [this] { copy(); });
    g->addColumn({btnCut, btnCopy});

    g = home->addGroup(new RibbonGroup(QStringLiteral("이미지")));
    QMenu *effectsMenu = menu({
        {QStringLiteral("흐리게..."), QStringLiteral("blur"), [this] { fxBlur(); }},
        {QStringLiteral("선명하게"), QString(),
         [this] {
             applyEffect(QStringLiteral("선명하게"),
                         [](const QImage &img, const QRect &r) { return effects::sharpen(img, 1.0, r); });
         }},
        {QStringLiteral("모자이크..."), QStringLiteral("mosaic"), [this] { fxMosaic(); }},
        separator(),
        {QStringLiteral("무채화"), QString(),
         [this] {
             applyEffect(QStringLiteral("무채화"), [](const QImage &img, const QRect &r) { return effects::grayscale(img, r); });
         }},
        {QStringLiteral("색반전"), QString(),
         [this] {
             applyEffect(QStringLiteral("색반전"), [](const QImage &img, const QRect &r) { return effects::invert(img, r); });
         }},
        {QStringLiteral("밝기/대비..."), QString(), [this] { fxBrightness(); }},
        {QStringLiteral("색조/채도..."), QString(), [this] { fxHue(); }},
        separator(),
        {QStringLiteral("테두리..."), QString(), [this] { fxBorder(); }},
        {QStringLiteral("그림자"), QString(), [this] { fxShadow(); }},
        {QStringLiteral("워터마크..."), QString(), [this] { fxWatermark(); }},
    });
    auto *bFx = new RibbonButton(QStringLiteral("효과"), QStringLiteral("effect"), false, effectsMenu);
    auto *bResize = new RibbonButton(QStringLiteral("크기 조절"), QStringLiteral("resize"), false,
                                     menu({
                                         {QStringLiteral("이미지 크기 조절..."), QStringLiteral("resize"), [this] { resizeImage(); }},
                                         {QStringLiteral("캔버스 크기 조절..."), QString(), [this] { resizeCanvas(); }},
                                     }));
    auto rotateBy = [this](int deg) {
        applyTransform(QStringLiteral("회전"), [deg](const QImage &img) { return effects::rotate(img, deg); });
    };
    auto *bRotate = new RibbonButton(
        QStringLiteral("회전"), QStringLiteral("rotate"), false,
        menu({
            {QStringLiteral("오른쪽으로 90° 회전"), QString(), [rotateBy] { rotateBy(90); }},
            {QStringLiteral("왼쪽으로 90° 회전"), QString(), [rotateBy] { rotateBy(-90); }},
            {QStringLiteral("180° 회전"), QString(), [rotateBy] { rotateBy(180); }},
            separator(),
            {QStringLiteral("좌우 대칭"), QString(),
             [this] {
                 applyTransform(QStringLiteral("좌우 대칭"), [](const QImage &img) { return effects::flip(img, true); });
             }},
            {QStringLiteral("상하 대칭"), QString(),
             [this] {
                 applyTransform(QStringLiteral("상하 대칭"), [](const QImage &img) { return effects::flip(img, false); });
             }},
        }));
    g->addColumn({bFx, bResize, bRotate});
    auto *bCrop = new RibbonButton(QStringLiteral("자르기"), QStringLiteral("crop"), false);
    connect(bCrop, &QToolButton::clicked, this, [this] { crop(); });
    g->addColumn({bCrop});

    g = home->addGroup(new RibbonGroup(QStringLiteral("도구")));
    struct ToolEntry
    {
        QString key, label, icon;
        QMenu *menu;
    };
    const QList<ToolEntry> tools = {
        {QStringLiteral("move"), QStringLiteral("이동"), QStringLiteral("move"), nullptr},
        {QStringLiteral("select"), QStringLiteral("선택"), QStringLiteral("select"),
         menu({
             {QStringLiteral("사각형 선택"), QStringLiteral("select"), [this] { setTool(QStringLiteral("select")); }},
             {QStringLiteral("모두 선택"), QString(), [this] { selectAll(); }},
             {QStringLiteral("선택 해제"), QString(), [this] { deselect(); }},
             separator(),
             {QStringLiteral("선택 영역 자르기"), QStringLiteral("crop"), [this] { crop(); }},
         })},
        {QStringLiteral("draw"), QStringLiteral("그리기"), QStringLiteral("draw"),
         menu({
             {QStringLiteral("연필"), QStringLiteral("draw"), [this] { setDrawKind(QStringLiteral("pen")); }},
             {QStringLiteral("형광펜"), QStringLiteral("highlighter"), [this] { setDrawKind(QStringLiteral("highlighter")); }},
             {QStringLiteral("지우개"), QStringLiteral("eraser"), [this] { setDrawKind(QStringLiteral("eraser")); }},
         })},
        {QStringLiteral("fill"), QStringLiteral("채우기"), QStringLiteral("fill"),
         menu({
             {QStringLiteral("채우기 허용 오차..."), QString(), [this] { fillToleranceDialog(); }},
         })},
        {QStringLiteral("text"), QStringLiteral("텍스트"), QStringLiteral("text"),
         menu({
             {QStringLiteral("텍스트 입력"), QStringLiteral("text"), [this] { setTool(QStringLiteral("text")); }},
             {QStringLiteral("글꼴 선택..."), QString(), [this] { pickFont(); }},
         })},
        {QStringLiteral("stamp"), QStringLiteral("스탬프"), QStringLiteral("stamp"),
         menu({
             {QStringLiteral("번호 스탬프"), QStringLiteral("number_stamp"), [this] { setStampKind(QStringLiteral("number")); }},
             {QStringLiteral("커서 스탬프"), QStringLiteral("cursor_stamp"), [this] { setStampKind(QStringLiteral("cursor")); }},
             separator(),
             {QStringLiteral("번호 초기화"), QString(), [this] { resetStampCounter(); }},
             {QStringLiteral("스탬프 크기..."), QString(), [this] { stampSizeDialog(); }},
         })},
        {QStringLiteral("shape"), QStringLiteral("도형"), QStringLiteral("shape"), shapeMenu()},
    };
    for (const ToolEntry &t : tools) {
        auto *btn = new RibbonButton(t.label, t.icon, true, t.menu, true);
        const QString key = t.key;
        connect(btn, &QToolButton::clicked, this, [this, key] { setTool(key); });
        g->add(btn);
        m_toolButtons.append({key, btn});
    }

    g = home->addGroup(new RibbonGroup(QStringLiteral("크기")));
    auto *widthMenu = new QMenu(this);
    m_widthGroup = new QActionGroup(this);
    for (int w : {1, 2, 3, 5, 8, 12, 16}) {
        QAction *a = widthMenu->addAction(QStringLiteral("%1 px").arg(w));
        a->setCheckable(true);
        a->setChecked(w == m_state.width);
        a->setData(w);
        m_widthGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, w] { setLineWidth(w); });
    }
    m_btnWidth = new RibbonButton(QStringLiteral("%1px").arg(m_state.width), QStringLiteral("line_width"), true, widthMenu);
    g->add(m_btnWidth);

    g = home->addGroup(new RibbonGroup(QStringLiteral("색상")));
    m_sw1 = new ColorSwatch(QStringLiteral("색1"), m_state.color1);
    m_sw1->setChecked(true);
    connect(m_sw1, &QToolButton::clicked, this, [this] { swatchClicked(true); });
    m_sw2 = new ColorSwatch(QStringLiteral("색2"), m_state.color2);
    connect(m_sw2, &QToolButton::clicked, this, [this] { swatchClicked(false); });
    g->add(m_sw1);
    g->add(m_sw2);
    auto *bSwap = new RibbonButton(QString(), QStringLiteral("swap"), false);
    bSwap->setToolTip(QStringLiteral("색1/색2 바꾸기"));
    connect(bSwap, &QToolButton::clicked, this, [this] { swapColors(); });
    auto *bReset = new RibbonButton(QString(), QStringLiteral("reset_colors"), false);
    bReset->setToolTip(QStringLiteral("기본 색상"));
    connect(bReset, &QToolButton::clicked, this, [this] { resetColors(); });
    auto *bPick = new RibbonButton(QString(), QStringLiteral("eyedropper"), false, nullptr, true);
    bPick->setToolTip(QStringLiteral("스포이트 (캔버스에서 색 추출)"));
    connect(bPick, &QToolButton::clicked, this, [this] { setTool(QStringLiteral("eyedropper")); });
    m_toolButtons.append({QStringLiteral("eyedropper"), bPick});
    g->addColumn({bReset, bSwap, bPick});

    g = home->addGroup(new RibbonGroup(QStringLiteral("팔레트")));
    m_palette = new PaletteGrid(m_config.list(QStringLiteral("palette"), QStringLiteral("custom")));
    connect(m_palette, &PaletteGrid::picked, this, &EditorWindow::palettePicked);
    g->add(m_palette);
    auto *more = new RibbonButton(QStringLiteral("더보기"), QStringLiteral("more_colors"));
    connect(more, &QToolButton::clicked, this, [this] { moreColors(); });
    g->add(more);
    rb->addPage(QStringLiteral("홈"), home);

    // Share ---------------------------------------------------------------
    auto *sharePage = new RibbonPage();
    g = sharePage->addGroup(new RibbonGroup(QStringLiteral("저장")));
    auto *bSave = new RibbonButton(QStringLiteral("저장"), QStringLiteral("save"));
    connect(bSave, &QToolButton::clicked, this, [this] { save(); });
    g->add(bSave);
    auto *bSaveAs = new RibbonButton(QStringLiteral("다른 이름으로"), QStringLiteral("save_as"));
    connect(bSaveAs, &QToolButton::clicked, this, [this] { saveAs(); });
    g->add(bSaveAs);
    auto *bPrint = new RibbonButton(QStringLiteral("인쇄"), QStringLiteral("print"));
    connect(bPrint, &QToolButton::clicked, this, [this] { printImage(); });
    g->add(bPrint);
    g = sharePage->addGroup(new RibbonGroup(QStringLiteral("보내기")));
    const QList<std::tuple<QString, QString, QString>> targets = {
        {QStringLiteral("클립보드"), QStringLiteral("copy"), QStringLiteral("clipboard")},
        {QStringLiteral("이메일"), QStringLiteral("email"), QStringLiteral("email")},
        {QStringLiteral("FTP"), QStringLiteral("ftp"), QStringLiteral("ftp")},
        {QStringLiteral("프로그램"), QStringLiteral("program"), QStringLiteral("program")},
        {QStringLiteral("기본 앱"), QStringLiteral("open"), QStringLiteral("default_app")},
    };
    for (const auto &[text, ic, key] : targets) {
        auto *b = new RibbonButton(text, ic);
        const QString target = key;
        connect(b, &QToolButton::clicked, this, [this, target] { share(target); });
        g->add(b);
    }
    rb->addPage(QStringLiteral("공유"), sharePage);

    // View ----------------------------------------------------------------
    auto *view = new RibbonPage();
    g = view->addGroup(new RibbonGroup(QStringLiteral("확대/축소")));
    for (QAction *a : {m_aZoomIn, m_aZoomOut, m_aZoom100, m_aZoomFit}) {
        auto *b = new RibbonButton(a->text(), a->icon());
        connect(b, &QToolButton::clicked, a, &QAction::trigger);
        g->add(b);
    }
    g = view->addGroup(new RibbonGroup(QStringLiteral("표시")));
    auto *bBg = new RibbonButton(QStringLiteral("배경"), QStringLiteral("grid"), true,
                                 menu({
                                     {QStringLiteral("단색 배경"), QString(), [this] { setBgMode(QStringLiteral("solid")); }},
                                     {QStringLiteral("격자 배경"), QStringLiteral("grid"), [this] { setBgMode(QStringLiteral("grid")); }},
                                 }));
    g->add(bBg);
    auto *bStatus = new RibbonButton(QStringLiteral("상태 표시줄"), QStringLiteral("thumbnail"), true, nullptr, true);
    bStatus->setChecked(true);
    connect(bStatus, &QToolButton::toggled, this, &EditorWindow::toggleStatus);
    g->add(bStatus);
    g = view->addGroup(new RibbonGroup(QStringLiteral("개체")));
    auto *bMerge = new RibbonButton(QStringLiteral("개체 병합"), QStringLiteral("thumbnail"));
    connect(bMerge, &QToolButton::clicked, this, [this] { mergeObjects(); });
    g->add(bMerge);
    g = view->addGroup(new RibbonGroup(QStringLiteral("화면 캡처")));
    QList<MenuEntry> capEntries;
    for (const CaptureModeInfo &m : captureModes()) {
        const QString id = m.id;
        capEntries.append({m.label, m.icon, [this, id] { m_ctx.capture(id); }});
    }
    auto *bCapture = new RibbonButton(QStringLiteral("캡처"), QStringLiteral("cap_region"), true, menu(capEntries));
    g->add(bCapture);
    QList<MenuEntry> toolEntries;
    for (const ToolInfo &t : graphicTools()) {
        const QString id = t.id;
        toolEntries.append({t.title, t.icon, [this, id] { m_ctx.openTool(id); }});
    }
    auto *bTools = new RibbonButton(QStringLiteral("그래픽 도구"), QStringLiteral("tool_color_picker"), true, menu(toolEntries));
    g->add(bTools);
    rb->addPage(QStringLiteral("보기"), view);

    connect(rb, &Ribbon::fileClicked, this, [this] { showBackstage(QStringLiteral("start")); });
    m_docRibbonKeep = {heart, bCapture, bTools, pasteBtn};
}

QMenu *EditorWindow::shapeMenu()
{
    auto *m = new QMenu(this);
    for (const QString &kind : shapeKinds()) {
        QAction *a = m->addAction(icons::icon(kind), shapeLabel(kind));
        connect(a, &QAction::triggered, this, [this, kind] { setShapeKind(kind); });
    }
    m->addSeparator();
    m_aShapeFill = m->addAction(QStringLiteral("도형 채우기 (색2)"));
    m_aShapeFill->setCheckable(true);
    connect(m_aShapeFill, &QAction::toggled, this, [this](bool on) { m_state.shapeFill = on; });
    return m;
}

void EditorWindow::toggleStatus(bool on)
{
    m_statusVisible = on;
    statusBar()->setVisible(on);
}

void EditorWindow::buildStatus()
{
    m_statusVisible = true;
    auto *sb = new QStatusBar();
    setStatusBar(sb);
    m_lblSize = new QLabel();
    m_lblPos = new QLabel();
    m_lblSel = new QLabel();
    sb->addWidget(m_lblSize);
    sb->addWidget(m_lblPos);
    sb->addWidget(m_lblSel);
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(5, 800);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) { setZoom(v / 100.0); });
    m_lblZoom = new QLabel(QStringLiteral("100%"));
    m_lblZoom->setMinimumWidth(48);
    sb->addPermanentWidget(m_zoomSlider);
    sb->addPermanentWidget(m_lblZoom);
}

void EditorWindow::applySettings()
{
    QPalette pal = m_empty->palette();
    pal.setColor(m_empty->backgroundRole(), QColor(m_config.str(kEditor, QStringLiteral("bg_color"), QStringLiteral("#dcdcdc"))));
    m_empty->setPalette(pal);
    const bool centered = m_config.flag(kEditor, QStringLiteral("center_image"), false);
    for (CanvasView *view : views()) {
        view->setBackgroundBrush(bgBrush());
        view->setAlignment(centered ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignTop));
    }
    m_palette->setCustom(m_config.list(QStringLiteral("palette"), QStringLiteral("custom")));
}

bool EditorWindow::autoMergeOnSelect() const
{
    return m_config.flag(kEditor, QStringLiteral("auto_shape_selection"), false);
}

QBrush EditorWindow::bgBrush() const
{
    if (m_config.str(kEditor, QStringLiteral("bg_mode"), QStringLiteral("solid")) == QLatin1String("grid"))
        return gridBrush();
    return QBrush(QColor(m_config.str(kEditor, QStringLiteral("bg_color"), QStringLiteral("#dcdcdc"))));
}

// ---------------------------------------------------------------- documents

bool EditorWindow::hasDocument() const
{
    return currentDocument() != nullptr;
}

int EditorWindow::documentCount() const
{
    return m_tabs->count();
}

CanvasView *EditorWindow::currentView() const
{
    return qobject_cast<CanvasView *>(m_tabs->currentWidget());
}

Document *EditorWindow::currentDocument() const
{
    CanvasView *v = currentView();
    return v ? v->document() : nullptr;
}

CanvasView *EditorWindow::viewAt(int index) const
{
    return qobject_cast<CanvasView *>(m_tabs->widget(index));
}

QList<CanvasView *> EditorWindow::views() const
{
    QList<CanvasView *> out;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (CanvasView *v = viewAt(i))
            out << v;
    }
    return out;
}

void EditorWindow::setCurrentIndex(int index)
{
    m_tabs->setCurrentIndex(index);
}

RibbonButton *EditorWindow::toolButton(const QString &tool) const
{
    for (const auto &[key, btn] : m_toolButtons) {
        if (key == tool)
            return btn;
    }
    return nullptr;
}

Document *EditorWindow::addImage(const QImage &image, const QString &path, const QString &title)
{
    QString t = title;
    if (t.isEmpty() && path.isEmpty())
        t = QStringLiteral("이미지 %1").arg(m_tabs->count() + 1);
    auto *doc = new Document(image, path, t);
    auto *view = new CanvasView(doc, &m_state, [this] { return nextStamp(); }, bgBrush(),
                                m_config.flag(kEditor, QStringLiteral("center_image"), false));
    view->setAutoMergeProvider([this] { return autoMergeOnSelect(); });
    connect(view, &CanvasView::cursorMoved, this,
            [this](const QPoint &p) { m_lblPos->setText(QStringLiteral("  %1, %2 px").arg(p.x()).arg(p.y())); });
    connect(view, &CanvasView::zoomChanged, this, [this, view](double z) {
        if (view == currentView())
            onZoomChanged(z);
    });
    connect(view, &CanvasView::colorPicked, this, [this](const QColor &c, bool primary) { setColor(c, primary); });
    connect(view, &CanvasView::toolFinished, this, &EditorWindow::syncToolButtons);
    connect(doc, &Document::changed, this, &EditorWindow::updateUi);
    connect(doc, &Document::selectionChanged, this, &EditorWindow::updateUi);
    connect(doc->undoStack(), &QUndoStack::cleanChanged, this, &EditorWindow::updateTitles);
    const int idx = m_tabs->addTab(view, doc->title());
    m_tabs->setCurrentIndex(idx);
    hideBackstage();
    QTimer::singleShot(0, view, [this, view] { initialZoom(view); });
    updateUi();
    return doc;
}

void EditorWindow::initialZoom(CanvasView *view)
{
    const QSize vp = view->viewport()->size();
    const QRectF r = view->document()->scene()->sceneRect();
    if (r.width() > vp.width() || r.height() > vp.height())
        view->fit();
    else
        view->setZoom(1.0);
}

bool EditorWindow::openPath(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, appName(),
                             QStringLiteral("이미지를 열 수 없습니다.\n%1\n%2").arg(path, reader.errorString()));
        return false;
    }
    const QString abs = QFileInfo(path).absoluteFilePath();
    Document *doc = addImage(img, abs);
    doc->undoStack()->setClean();
    m_config.addRecentFile(abs);
    m_config.save();
    return true;
}

void EditorWindow::openFiles()
{
    const QStringList recent = m_config.list(QStringLiteral("recent"), QStringLiteral("files"));
    const QString folder = recent.isEmpty() ? picturesDir() : QFileInfo(recent.first()).absolutePath();
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("열기"), folder, imageOpenFilter());
    for (const QString &p : paths)
        openPath(p);
}

bool EditorWindow::closeDocument(int index)
{
    if (index < 0)
        return true;
    CanvasView *view = viewAt(index);
    if (!view)
        return true;
    Document *doc = view->document();
    if (doc->isModified() && !m_config.flag(kEditor, QStringLiteral("no_save_prompt"), false)) {
        m_tabs->setCurrentIndex(index);
        const auto ans = QMessageBox::question(
            this, appName(), QStringLiteral("'%1' 의 변경 내용을 저장하시겠습니까?").arg(doc->title()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ans == QMessageBox::Cancel)
            return false;
        if (ans == QMessageBox::Save && !save())
            return false;
    }
    view->disconnect(this);
    doc->disconnect(this);
    doc->undoStack()->disconnect(this);
    m_tabs->removeTab(m_tabs->indexOf(view));
    view->deleteLater();
    updateUi();
    return true;
}

bool EditorWindow::closeAll()
{
    while (m_tabs->count()) {
        if (!closeDocument(m_tabs->count() - 1))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------- file

bool EditorWindow::saveTo(Document *doc, const QString &path)
{
    QString p = path;
    if (extOf(p).isEmpty())
        p += QLatin1Char('.') + m_config.str(QStringLiteral("filename"), QStringLiteral("format"), QStringLiteral("png"));
    if (!outputs::saveImage(doc->flatten(), p, m_config)) {
        QMessageBox::warning(this, appName(), QStringLiteral("저장하지 못했습니다.\n%1").arg(p));
        return false;
    }
    doc->setPath(p);
    doc->setTitle(QFileInfo(p).fileName());
    doc->undoStack()->setClean();
    m_config.addRecentFile(p);
    m_config.save();
    updateTitles();
    showMessage(QStringLiteral("저장됨: %1").arg(p), 4000);
    return true;
}

bool EditorWindow::save()
{
    Document *doc = currentDocument();
    if (!doc)
        return false;
    if (!doc->path().isEmpty()) {
        const QString ext = extOf(doc->path());
        for (const auto &f : imageFormats()) {
            if (f.first == ext)
                return saveTo(doc, doc->path());
        }
    }
    return saveAs();
}

bool EditorWindow::saveAs()
{
    Document *doc = currentDocument();
    if (!doc)
        return false;
    const QString fmt = m_config.str(QStringLiteral("filename"), QStringLiteral("format"), QStringLiteral("png"));
    QString selected = imageFormats().first().second;
    for (const auto &f : imageFormats()) {
        if (f.first == fmt)
            selected = f.second;
    }
    QString suggestion = doc->path();
    if (suggestion.isEmpty()) {
        const outputs::GeneratedName name = outputs::nextFilename(m_config);
        QString folder = m_config.str(QStringLiteral("autosave"), QStringLiteral("folder"));
        if (folder.isEmpty())
            folder = picturesDir();
        suggestion = QDir(folder).filePath(name.name + QLatin1Char('.') + name.ext);
    }
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("다른 이름으로 저장"), suggestion,
                                                imageSaveFilter(), &selected);
    if (path.isEmpty())
        return false;
    if (extOf(path).isEmpty()) {
        for (const auto &f : imageFormats()) {
            if (f.second == selected) {
                path += QLatin1Char('.') + f.first;
                break;
            }
        }
    }
    return saveTo(doc, path);
}

void EditorWindow::printImage()
{
    Document *doc = currentDocument();
    if (!doc)
        return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QImage img = doc->flatten();
    QPainter painter(&printer);
    const QRect rect = painter.viewport();
    QSize size = img.size();
    size.scale(rect.size(), Qt::KeepAspectRatio);
    painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
    painter.setWindow(img.rect());
    painter.drawImage(0, 0, img);
    painter.end();
}

void EditorWindow::share(const QString &target)
{
    Document *doc = currentDocument();
    if (!doc)
        return;
    m_ctx.shareImage(doc->flatten(), target, this, doc->path());
}

// ---------------------------------------------------------------- edit

void EditorWindow::undo()
{
    if (Document *d = currentDocument())
        d->undoStack()->undo();
}

void EditorWindow::redo()
{
    if (Document *d = currentDocument())
        d->undoStack()->redo();
}

void EditorWindow::copy()
{
    if (Document *d = currentDocument()) {
        QGuiApplication::clipboard()->setImage(d->selectionImage());
        showMessage(QStringLiteral("클립보드에 복사했습니다."), 2500);
    }
}

void EditorWindow::cut()
{
    Document *d = currentDocument();
    if (!d)
        return;
    copy();
    if (d->hasSelection()) {
        const QRect rect = d->selection();
        const QColor color = m_state.color2;
        d->applyRaster([rect, color](const QImage &img) { return effects::fillRect(img, rect, color); },
                       QStringLiteral("잘라내기"));
    }
}

void EditorWindow::deleteSelection()
{
    Document *d = currentDocument();
    if (!d)
        return;
    const QList<QGraphicsItem *> items = d->scene()->selectedItems();
    if (!items.isEmpty()) {
        d->removeItems(items);
    } else if (d->hasSelection()) {
        const QRect rect = d->selection();
        const QColor color = m_state.color2;
        d->applyRaster([rect, color](const QImage &img) { return effects::fillRect(img, rect, color); },
                       QStringLiteral("삭제"));
    }
}

void EditorWindow::paste()
{
    const QImage img = QGuiApplication::clipboard()->image();
    if (img.isNull()) {
        showMessage(QStringLiteral("클립보드에 이미지가 없습니다."), 2500);
        return;
    }
    if (CanvasView *v = currentView())
        v->pastePixmap(QPixmap::fromImage(img));
    else
        addImage(img, QString(), QStringLiteral("클립보드"));
}

void EditorWindow::pasteAsNew()
{
    const QImage img = QGuiApplication::clipboard()->image();
    if (img.isNull()) {
        showMessage(QStringLiteral("클립보드에 이미지가 없습니다."), 2500);
        return;
    }
    addImage(img, QString(), QStringLiteral("클립보드"));
}

void EditorWindow::selectAll()
{
    if (Document *d = currentDocument())
        d->setSelection(d->image().rect());
}

void EditorWindow::deselect()
{
    if (Document *d = currentDocument()) {
        d->clearSelection();
        d->scene()->clearSelection();
    }
}

void EditorWindow::crop()
{
    Document *d = currentDocument();
    if (!d)
        return;
    if (!d->cropSelection())
        showMessage(QStringLiteral("먼저 '선택' 도구로 자를 영역을 지정하세요."), 3000);
}

void EditorWindow::mergeObjects()
{
    if (Document *d = currentDocument())
        d->mergeObjects();
}

// ---------------------------------------------------------------- effects

void EditorWindow::applyEffect(const QString &name, const EffectFn &fn)
{
    Document *d = currentDocument();
    if (!d)
        return;
    const QRect rect = d->selection();
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    d->applyRaster([fn, rect](const QImage &img) { return fn(img, rect); }, name);
    QGuiApplication::restoreOverrideCursor();
}

void EditorWindow::applyTransform(const QString &name, const Document::RasterFn &fn)
{
    Document *d = currentDocument();
    if (!d)
        return;
    d->clearSelection();
    d->applyRaster(fn, name);
}

void EditorWindow::fxBlur()
{
    const auto v = ask(QStringLiteral("흐리게"), {ParamField::integer(QStringLiteral("radius"), QStringLiteral("강도"), 4, 1, 40)}, this);
    if (!v)
        return;
    const int radius = v->value(QStringLiteral("radius")).toInt();
    applyEffect(QStringLiteral("흐리게"), [radius](const QImage &img, const QRect &r) { return effects::blur(img, radius, r); });
}

void EditorWindow::fxMosaic()
{
    const auto v = ask(QStringLiteral("모자이크"), {ParamField::integer(QStringLiteral("block"), QStringLiteral("블록 크기"), 10, 2, 80)}, this);
    if (!v)
        return;
    const int block = v->value(QStringLiteral("block")).toInt();
    applyEffect(QStringLiteral("모자이크"), [block](const QImage &img, const QRect &r) { return effects::mosaic(img, block, r); });
}

void EditorWindow::fxBrightness()
{
    const auto v = ask(QStringLiteral("밝기/대비"),
                       {ParamField::integer(QStringLiteral("b"), QStringLiteral("밝기"), 0, -100, 100),
                        ParamField::integer(QStringLiteral("c"), QStringLiteral("대비"), 0, -100, 100)},
                       this);
    if (!v)
        return;
    const int b = v->value(QStringLiteral("b")).toInt();
    const int c = v->value(QStringLiteral("c")).toInt();
    applyEffect(QStringLiteral("밝기/대비"),
                [b, c](const QImage &img, const QRect &r) { return effects::brightnessContrast(img, b, c, r); });
}

void EditorWindow::fxHue()
{
    const auto v = ask(QStringLiteral("색조/채도"),
                       {ParamField::integer(QStringLiteral("h"), QStringLiteral("색조"), 0, -180, 180),
                        ParamField::integer(QStringLiteral("s"), QStringLiteral("채도"), 0, -100, 100),
                        ParamField::integer(QStringLiteral("l"), QStringLiteral("밝기"), 0, -100, 100)},
                       this);
    if (!v)
        return;
    const int h = v->value(QStringLiteral("h")).toInt();
    const int s = v->value(QStringLiteral("s")).toInt();
    const int l = v->value(QStringLiteral("l")).toInt();
    applyEffect(QStringLiteral("색조/채도"),
                [h, s, l](const QImage &img, const QRect &r) { return effects::hueSaturation(img, h, s, l, r); });
}

void EditorWindow::fxBorder()
{
    const auto v = ask(QStringLiteral("테두리"),
                       {ParamField::integer(QStringLiteral("w"), QStringLiteral("두께"), 4, 1, 100),
                        ParamField::color(QStringLiteral("color"), QStringLiteral("색상"), m_state.color1),
                        ParamField::boolean(QStringLiteral("inside"), QStringLiteral("이미지 안쪽에 그리기"), false)},
                       this);
    if (!v)
        return;
    const int w = v->value(QStringLiteral("w")).toInt();
    const QColor color = v->value(QStringLiteral("color")).value<QColor>();
    const bool inside = v->value(QStringLiteral("inside")).toBool();
    applyTransform(QStringLiteral("테두리"),
                   [w, color, inside](const QImage &img) { return effects::border(img, w, color, inside); });
}

void EditorWindow::fxShadow()
{
    applyTransform(QStringLiteral("그림자"), [](const QImage &img) { return effects::dropShadow(img); });
}

void EditorWindow::fxWatermark()
{
    const auto v = ask(QStringLiteral("워터마크"),
                       {
                           ParamField::text(QStringLiteral("text"), QStringLiteral("텍스트"), QStringLiteral("MikMick")),
                           ParamField::integer(QStringLiteral("size"), QStringLiteral("글자 크기"), 28, 8, 200),
                           ParamField::real(QStringLiteral("opacity"), QStringLiteral("불투명도"), 0.35, 0.05, 1.0),
                           ParamField::color(QStringLiteral("color"), QStringLiteral("색상"), QColor(Qt::white)),
                           ParamField::choice(QStringLiteral("pos"), QStringLiteral("위치"), QStringLiteral("bottom-right"),
                                              {
                                                  {QStringLiteral("bottom-right"), QStringLiteral("오른쪽 아래")},
                                                  {QStringLiteral("bottom-left"), QStringLiteral("왼쪽 아래")},
                                                  {QStringLiteral("top-right"), QStringLiteral("오른쪽 위")},
                                                  {QStringLiteral("top-left"), QStringLiteral("왼쪽 위")},
                                                  {QStringLiteral("center"), QStringLiteral("가운데")},
                                                  {QStringLiteral("tile"), QStringLiteral("바둑판 배열")},
                                              }),
                       },
                       this);
    if (!v)
        return;
    const QString text = v->value(QStringLiteral("text")).toString();
    if (text.isEmpty())
        return;
    const QColor color = v->value(QStringLiteral("color")).value<QColor>();
    const double opacity = v->value(QStringLiteral("opacity")).toDouble();
    const QString pos = v->value(QStringLiteral("pos")).toString();
    const int size = v->value(QStringLiteral("size")).toInt();
    applyTransform(QStringLiteral("워터마크"), [=](const QImage &img) {
        return effects::watermark(img, text, color, opacity, pos, size);
    });
}

void EditorWindow::resizeImage()
{
    Document *d = currentDocument();
    if (!d)
        return;
    ResizeDialog dlg(d->size(), QStringLiteral("이미지 크기 조절"), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QSize s = dlg.resultSize();
    applyTransform(QStringLiteral("크기 조절"), [s](const QImage &img) { return effects::resize(img, s.width(), s.height()); });
}

void EditorWindow::resizeCanvas()
{
    Document *d = currentDocument();
    if (!d)
        return;
    ResizeDialog dlg(d->size(), QStringLiteral("캔버스 크기 조절"), this, true);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QSize s = dlg.resultSize();
    const QString anchor = dlg.anchor();
    const QColor fill = dlg.fillColor();
    applyTransform(QStringLiteral("캔버스 크기"), [s, anchor, fill](const QImage &img) {
        return effects::canvasSize(img, s.width(), s.height(), anchor, fill);
    });
}

// ---------------------------------------------------------------- tool state

void EditorWindow::setTool(const QString &tool)
{
    m_state.tool = tool;
    for (CanvasView *v : views())
        v->applyTool();
    syncToolButtons();
}

void EditorWindow::syncToolButtons()
{
    for (const auto &[key, btn] : std::as_const(m_toolButtons))
        btn->setChecked(key == m_state.tool);
}

void EditorWindow::setDrawKind(const QString &kind)
{
    m_state.drawKind = kind;
    QString ic = QStringLiteral("draw");
    if (kind == QLatin1String("highlighter"))
        ic = QStringLiteral("highlighter");
    else if (kind == QLatin1String("eraser"))
        ic = QStringLiteral("eraser");
    toolButton(QStringLiteral("draw"))->setIcon(icons::icon(ic));
    setTool(QStringLiteral("draw"));
}

void EditorWindow::setShapeKind(const QString &kind)
{
    m_state.shapeKind = kind;
    toolButton(QStringLiteral("shape"))->setIcon(icons::icon(kind));
    setTool(QStringLiteral("shape"));
}

void EditorWindow::setStampKind(const QString &kind)
{
    m_state.stampKind = kind;
    toolButton(QStringLiteral("stamp"))
        ->setIcon(icons::icon(kind == QLatin1String("number") ? QStringLiteral("number_stamp") : QStringLiteral("cursor_stamp")));
    setTool(QStringLiteral("stamp"));
}

int EditorWindow::nextStamp()
{
    return ++m_stampNo;
}

void EditorWindow::resetStampCounter()
{
    m_stampNo = 0;
    showMessage(QStringLiteral("스탬프 번호를 1부터 다시 시작합니다."), 2500);
}

void EditorWindow::stampSizeDialog()
{
    const auto v = ask(QStringLiteral("스탬프 크기"),
                       {ParamField::integer(QStringLiteral("s"), QStringLiteral("크기"), m_state.stampSize, 12, 120)}, this);
    if (v)
        m_state.stampSize = v->value(QStringLiteral("s")).toInt();
}

void EditorWindow::fillToleranceDialog()
{
    const auto v = ask(QStringLiteral("채우기 허용 오차"),
                       {ParamField::integer(QStringLiteral("t"), QStringLiteral("허용 오차"), m_state.fillTolerance, 0, 255)},
                       this);
    if (v) {
        m_state.fillTolerance = v->value(QStringLiteral("t")).toInt();
        setTool(QStringLiteral("fill"));
    }
}

void EditorWindow::pickFont()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, m_state.font, this, QStringLiteral("글꼴"));
    if (!ok)
        return;
    m_state.font = font;
    if (Document *d = currentDocument()) {
        const QList<QGraphicsItem *> selected = d->scene()->selectedItems();
        for (QGraphicsItem *item : selected) {
            if (auto *text = qgraphicsitem_cast<TextItem *>(item))
                text->setFont(font);
        }
    }
    setTool(QStringLiteral("text"));
}

void EditorWindow::setLineWidth(int width)
{
    m_state.width = width;
    m_btnWidth->setText(QStringLiteral("%1px").arg(width));
    for (QAction *a : m_widthGroup->actions())
        a->setChecked(a->data().toInt() == width);
}

void EditorWindow::swatchClicked(bool first)
{
    m_sw1->setChecked(first);
    m_sw2->setChecked(!first);
    const QColor current = first ? m_state.color1 : m_state.color2;
    const QColor c = QColorDialog::getColor(current, this, first ? QStringLiteral("색1") : QStringLiteral("색2"),
                                            QColorDialog::ShowAlphaChannel);
    if (c.isValid())
        setColor(c, first);
}

void EditorWindow::setColor(const QColor &c, bool first)
{
    if (first) {
        m_state.color1 = c;
        m_sw1->setColor(c);
    } else {
        m_state.color2 = c;
        m_sw2->setColor(c);
    }
    Document *d = currentDocument();
    if (!d || !first)
        return;
    const QList<QGraphicsItem *> selected = d->scene()->selectedItems();
    for (QGraphicsItem *item : selected) {
        if (auto *shape = qgraphicsitem_cast<ShapeItem *>(item))
            shape->setColor(c);
        else if (auto *stamp = qgraphicsitem_cast<StampItem *>(item))
            stamp->setColor(c);
        else if (auto *text = qgraphicsitem_cast<TextItem *>(item))
            text->setDefaultTextColor(c);
    }
}

void EditorWindow::palettePicked(const QColor &c, bool primary)
{
    const bool first = !m_sw2->isChecked() ? primary : !primary;
    setColor(c, first);
}

void EditorWindow::swapColors()
{
    const QColor c1 = m_state.color1;
    const QColor c2 = m_state.color2;
    setColor(c2, true);
    setColor(c1, false);
}

void EditorWindow::resetColors()
{
    setColor(QColor(Qt::black), true);
    setColor(QColor(Qt::white), false);
}

void EditorWindow::moreColors()
{
    const QColor c = QColorDialog::getColor(m_state.color1, this, QStringLiteral("색 편집"), QColorDialog::ShowAlphaChannel);
    if (!c.isValid())
        return;
    QStringList custom = m_config.list(QStringLiteral("palette"), QStringLiteral("custom"));
    custom.removeAll(c.name());
    custom.prepend(c.name());
    custom = custom.mid(0, 10);
    m_config.set(QStringLiteral("palette"), QStringLiteral("custom"), custom);
    m_config.save();
    m_palette->setCustom(custom);
    setColor(c, true);
}

void EditorWindow::setBgMode(const QString &mode)
{
    m_config.set(kEditor, QStringLiteral("bg_mode"), mode);
    m_config.save();
    applySettings();
}

// ---------------------------------------------------------------- zoom

void EditorWindow::setZoom(double zoom)
{
    if (CanvasView *v = currentView())
        v->setZoom(zoom);
}

void EditorWindow::zoomBy(double factor)
{
    if (CanvasView *v = currentView())
        v->setZoom(v->zoom() * factor);
}

void EditorWindow::fitToWindow()
{
    if (CanvasView *v = currentView())
        v->fit();
}

void EditorWindow::onZoomChanged(double zoom)
{
    const int pct = roundHalfEven(zoom * 100);
    m_lblZoom->setText(QStringLiteral("%1%").arg(pct));
    const QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(pct);
}

// ---------------------------------------------------------------- backstage

void EditorWindow::showBackstage(const QString &page)
{
    m_backstage->setHasDocument(currentDocument() != nullptr);
    m_backstage->showPage(page);
    if (page == QLatin1String("thumbnail"))
        refreshThumbnails();
    m_stack->setCurrentWidget(m_backstage);
    statusBar()->hide();
}

void EditorWindow::hideBackstage()
{
    m_stack->setCurrentIndex(0);
    statusBar()->setVisible(m_statusVisible);
}

bool EditorWindow::isBackstageVisible() const
{
    return m_stack->currentWidget() == m_backstage;
}

void EditorWindow::refreshThumbnails()
{
    QList<QPair<QString, QPixmap>> docs;
    for (CanvasView *v : views())
        docs.append({v->document()->title(), QPixmap::fromImage(v->document()->flatten())});
    m_backstage->thumbnailPage()->setDocuments(docs);
}

void EditorWindow::onBackstage(const QString &kind, const QString &name)
{
    if (kind == QLatin1String("create")) {
        const QString sizeText = name.section(QLatin1Char(':'), 0, 0);
        const QColor color(name.section(QLatin1Char(':'), 1));
        const int w = std::max(1, sizeText.section(QLatin1Char('x'), 0, 0).toInt());
        const int h = std::max(1, sizeText.section(QLatin1Char('x'), 1, 1).toInt());
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(color);
        const QString preset = m_backstage->newPage()->presetKey();
        const QString ni = QStringLiteral("new_image");
        m_config.set(ni, QStringLiteral("preset"), preset);
        m_config.set(ni, QStringLiteral("width"), w);
        m_config.set(ni, QStringLiteral("height"), h);
        m_config.set(ni, QStringLiteral("bg"), color.name(QColor::HexArgb));
        m_config.save();
        if (preset == QLatin1String("clipboard")) {
            const QImage clip = QGuiApplication::clipboard()->image();
            if (!clip.isNull() && clip.size() == QSize(w, h)) {
                QPainter p(&img);
                p.drawImage(0, 0, clip);
                p.end();
            }
        }
        addImage(img);
    } else if (kind == QLatin1String("open")) {
        hideBackstage();
        openFiles();
    } else if (kind == QLatin1String("save")) {
        hideBackstage();
        save();
    } else if (kind == QLatin1String("save_as")) {
        hideBackstage();
        saveAs();
    } else if (kind == QLatin1String("print")) {
        hideBackstage();
        printImage();
    } else if (kind == QLatin1String("close")) {
        closeDocument(m_tabs->currentIndex());
        if (m_tabs->count() == 0)
            showBackstage(QStringLiteral("start"));
        else
            hideBackstage();
    } else if (kind == QLatin1String("share")) {
        share(name);
    } else if (kind == QLatin1String("thumbnails")) {
        refreshThumbnails();
    } else if (kind == QLatin1String("activate")) {
        m_tabs->setCurrentIndex(name.toInt());
        hideBackstage();
    } else if (kind == QLatin1String("options")) {
        m_ctx.showOptions(this);
    } else if (kind == QLatin1String("capture")) {
        m_ctx.capture(name);
    } else if (kind == QLatin1String("tool")) {
        m_ctx.openTool(name);
    } else if (kind == QLatin1String("homepage")) {
        m_ctx.homepage();
    } else if (kind == QLatin1String("check_update")) {
        m_ctx.checkUpdates(true, this);
    }
}

// ---------------------------------------------------------------- state refresh

void EditorWindow::onTabChanged(int)
{
    if (CanvasView *v = currentView()) {
        onZoomChanged(v->zoom());
        v->applyTool();
    }
    updateUi();
}

void EditorWindow::updateTitles()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (CanvasView *v = viewAt(i)) {
            Document *d = v->document();
            m_tabs->setTabText(i, (d->undoStack()->isClean() ? QString() : QStringLiteral("*")) + d->title());
        }
    }
    Document *d = currentDocument();
    setWindowTitle(d ? QStringLiteral("%1 - %2").arg(d->title(), appName()) : appName());
}

void EditorWindow::updateUi()
{
    Document *d = currentDocument();
    const bool has = d != nullptr;
    m_canvasStack->setCurrentIndex(has ? 1 : 0);
    for (QAction *a : std::as_const(m_docActions))
        a->setEnabled(has);
    m_aUndo->setEnabled(has && d->undoStack()->canUndo());
    m_aRedo->setEnabled(has && d->undoStack()->canRedo());
    m_ribbon->setBodyEnabled(has, m_docRibbonKeep);
    m_zoomSlider->setEnabled(has);
    if (has) {
        m_lblSize->setText(QStringLiteral("  %1 x %2 px").arg(d->size().width()).arg(d->size().height()));
        const QRect sel = d->selection();
        m_lblSel->setText(d->hasSelection() ? QStringLiteral("  선택: %1 x %2").arg(sel.width()).arg(sel.height()) : QString());
    } else {
        m_lblSize->clear();
        m_lblPos->clear();
        m_lblSel->clear();
    }
    updateTitles();
    syncToolButtons();
}

void EditorWindow::showMessage(const QString &text, int ms)
{
    statusBar()->showMessage(text, ms);
}

// ---------------------------------------------------------------- window events

void EditorWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage())
        event->acceptProposedAction();
}

void EditorWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *md = event->mimeData();
    if (md->hasUrls()) {
        const QList<QUrl> urls = md->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile())
                openPath(url.toLocalFile());
        }
    } else if (md->hasImage()) {
        addImage(qvariant_cast<QImage>(md->imageData()), QString(), QStringLiteral("드롭한 이미지"));
    }
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (m_config.flag(kEditor, QStringLiteral("quit_on_close"), false) || !m_ctx.trayAvailable()) {
        if (closeAll()) {
            event->accept();
            m_ctx.quit();
        } else {
            event->ignore();
        }
        return;
    }
    if (!closeAll()) {
        event->ignore();
        return;
    }
    event->ignore();
    hide();
}

void EditorWindow::bringToFront()
{
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
}

void EditorWindow::centerOnScreen()
{
    QScreen *screen = QGuiApplication::screenAt(QPoint(x() + 10, y() + 10));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect g = screen->availableGeometry();
    move(g.center() - rect().center());
}

} // namespace mm::editor
