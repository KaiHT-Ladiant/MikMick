#include "editor/Backstage.h"

#include "Version.h"
#include "core/Catalog.h"
#include "editor/Ribbon.h"
#include "ui/Icons.h"

#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace mm::editor {

namespace {

QString backstageQss()
{
    return QStringLiteral(R"(
#Backstage { background: #f3f3f3; }
#BackstageSide { background: %1; }
#BackstageSide QPushButton {
    color: white; background: transparent; border: none; text-align: left; padding: 9px 18px; font-size: 12px;
}
#BackstageSide QPushButton:hover { background: rgba(255,255,255,45); }
#BackstageSide QPushButton:checked { background: rgba(255,255,255,70); }
#BackstageSide QPushButton:disabled { color: rgba(255,255,255,90); }
#BackstageSide QToolButton { background: transparent; border: none; }
QLabel#PageTitle { font-size: 27px; color: #444; font-weight: 300; }
QLabel#SectionTitle { font-size: 15px; color: %1; }
QLabel#ItemDesc { color: #555; font-size: 11px; }
QToolButton#TaskItem { border: 1px solid transparent; text-align: left; padding: 4px; background: transparent; }
QToolButton#TaskItem:hover { background: #dcebfc; border-color: #a9cdf5; }
QToolButton#BigButton { border: 1px solid #c8c8c8; background: #f8f8f8; padding: 6px; }
QToolButton#BigButton:hover { background: #dcebfc; border-color: #a9cdf5; }
)")
        .arg(QLatin1String(kAccent));
}

QLabel *pageTitle(const QString &text)
{
    auto *lab = new QLabel(text);
    lab->setObjectName(QStringLiteral("PageTitle"));
    return lab;
}

QLabel *sectionTitle(const QString &text)
{
    auto *lab = new QLabel(text);
    lab->setObjectName(QStringLiteral("SectionTitle"));
    return lab;
}

QFrame *hline()
{
    auto *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #d0d0d0;"));
    return line;
}

QFrame *vline()
{
    auto *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet(QStringLiteral("color: #d0d0d0;"));
    return line;
}

// BigButton followed by a title / description column.
QHBoxLayout *bigButtonRow(BigButton *btn, const QString &title, const QString &desc)
{
    auto *row = new QHBoxLayout();
    row->addWidget(btn);
    auto *texts = new QVBoxLayout();
    auto *t = new QLabel(title);
    t->setStyleSheet(QStringLiteral("font-size: 15px; color: #333;"));
    texts->addWidget(t);
    auto *d = new QLabel(desc);
    d->setObjectName(QStringLiteral("ItemDesc"));
    texts->addWidget(d);
    texts->addStretch(1);
    row->addLayout(texts);
    row->addStretch(1);
    return row;
}

} // namespace

// ---------------------------------------------------------------- TaskItem / BigButton

TaskItem::TaskItem(const QString &iconName, const QString &title, const QString &desc)
{
    setObjectName(QStringLiteral("TaskItem"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(desc.isEmpty() ? 40 : 46);
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 2, 6, 2);
    lay->setSpacing(12);
    auto *ic = new QLabel();
    ic->setPixmap(icons::render(iconName, 30));
    ic->setFixedSize(32, 32);
    lay->addWidget(ic);
    auto *texts = new QVBoxLayout();
    texts->setSpacing(0);
    auto *t = new QLabel(title);
    t->setStyleSheet(QStringLiteral("font-size: 12px; color: #222;"));
    texts->addWidget(t);
    QLabel *d = nullptr;
    if (!desc.isEmpty()) {
        d = new QLabel(desc);
        d->setObjectName(QStringLiteral("ItemDesc"));
        texts->addWidget(d);
    }
    lay->addLayout(texts, 1);
    ic->setAttribute(Qt::WA_TransparentForMouseEvents);
    t->setAttribute(Qt::WA_TransparentForMouseEvents);
}

BigButton::BigButton(const QString &iconName, const QString &text)
{
    setObjectName(QStringLiteral("BigButton"));
    setIcon(icons::icon(iconName));
    setIconSize(QSize(32, 32));
    setText(text);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setFixedSize(96, 76);
}

const QList<CanvasPreset> &canvasPresets()
{
    static const QList<CanvasPreset> presets = {
        {QStringLiteral("clipboard"), QStringLiteral("클립보드"), QSize()},
        {QStringLiteral("screen"), QStringLiteral("화면 크기"), QSize()},
        {QStringLiteral("640x480"), QStringLiteral("640 x 480"), QSize(640, 480)},
        {QStringLiteral("800x600"), QStringLiteral("800 x 600"), QSize(800, 600)},
        {QStringLiteral("1024x768"), QStringLiteral("1024 x 768"), QSize(1024, 768)},
        {QStringLiteral("1280x720"), QStringLiteral("1280 x 720 (HD)"), QSize(1280, 720)},
        {QStringLiteral("1920x1080"), QStringLiteral("1920 x 1080 (Full HD)"), QSize(1920, 1080)},
        {QStringLiteral("a4"), QStringLiteral("A4 (96 DPI)"), QSize(794, 1123)},
        {QStringLiteral("custom"), QStringLiteral("사용자 정의"), QSize()},
    };
    return presets;
}

// ---------------------------------------------------------------- StartPage

StartPage::StartPage(bool hideStart)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 30, 14);
    root->addWidget(pageTitle(QStringLiteral("작업을 선택하십시오.")));
    root->addSpacing(14);
    auto *cols = new QHBoxLayout();
    cols->setSpacing(20);

    auto *left = new QVBoxLayout();
    left->setSpacing(2);
    left->addWidget(sectionTitle(QStringLiteral("새 작업")));
    struct Entry
    {
        QString kind, icon, title, desc;
    };
    const QList<Entry> tasks = {
        {QStringLiteral("new"), QStringLiteral("new"), QStringLiteral("새로 만들기"), QStringLiteral("새 이미지를 만듭니다.")},
        {QStringLiteral("open"), QStringLiteral("open"), QStringLiteral("열기"),
         QStringLiteral("기존 이미지 파일을 선택하여 불러옵니다.")},
    };
    for (const Entry &e : tasks) {
        auto *item = new TaskItem(e.icon, e.title, e.desc);
        const QString kind = e.kind;
        connect(item, &QToolButton::clicked, this, [this, kind] { Q_EMIT action(kind, QString()); });
        left->addWidget(item);
    }
    left->addSpacing(4);
    left->addWidget(hline());
    left->addSpacing(4);
    left->addWidget(sectionTitle(QStringLiteral("화면 캡처 도구")));
    for (const CaptureModeInfo &mode : captureModes()) {
        auto *item = new TaskItem(mode.icon, mode.label, QString());
        item->setFixedHeight(38);
        const QString id = mode.id;
        connect(item, &QToolButton::clicked, this, [this, id] { Q_EMIT action(QStringLiteral("capture"), id); });
        left->addWidget(item);
    }
    left->addStretch(1);
    cols->addLayout(left, 1);

    cols->addWidget(vline());

    auto *right = new QVBoxLayout();
    right->setSpacing(2);
    right->addWidget(sectionTitle(QStringLiteral("그래픽 도구")));
    for (const ToolInfo &tool : graphicTools()) {
        auto *item = new TaskItem(tool.icon, tool.title, tool.description);
        const QString id = tool.id;
        connect(item, &QToolButton::clicked, this, [this, id] { Q_EMIT action(QStringLiteral("tool"), id); });
        right->addWidget(item);
    }
    right->addStretch(1);
    cols->addLayout(right, 1);
    root->addLayout(cols, 1);

    auto *bottom = new QHBoxLayout();
    bottom->addStretch(1);
    m_hideCheck = new QCheckBox(QStringLiteral("시작할 때 이 창을 표시 안함"));
    m_hideCheck->setChecked(hideStart);
    connect(m_hideCheck, &QCheckBox::toggled, this, &StartPage::hideChanged);
    bottom->addWidget(m_hideCheck);
    root->addLayout(bottom);
}

// ---------------------------------------------------------------- NewPage

NewPage::NewPage(const QString &preset, const QSize &size, const QColor &bg)
    : m_bg(bg)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 30, 20);
    root->addWidget(pageTitle(QStringLiteral("새로 만들기")));
    root->addSpacing(14);

    auto *head = new QHBoxLayout();
    auto *btn = new BigButton(QStringLiteral("new"), QStringLiteral("새로 만들기"));
    connect(btn, &QToolButton::clicked, this, [this] { Q_EMIT createRequested(canvasSize(), m_bg); });
    head->addWidget(btn);
    auto *info = new QVBoxLayout();
    auto *lab = new QLabel(QStringLiteral("새 이미지를 만듭니다."));
    lab->setStyleSheet(QStringLiteral("font-size: 15px; color: #333;"));
    info->addWidget(lab);
    m_sizeLabel = new QLabel();
    m_sizeLabel->setObjectName(QStringLiteral("ItemDesc"));
    info->addWidget(m_sizeLabel);
    info->addStretch(1);
    head->addLayout(info);
    head->addStretch(1);
    root->addLayout(head);
    root->addSpacing(6);
    QFrame *line = hline();
    line->setFixedWidth(290);
    root->addWidget(line);
    root->addSpacing(6);

    root->addWidget(sectionTitle(QStringLiteral("캔버스 크기")));
    root->addWidget(new QLabel(QStringLiteral("프리셋:")));
    m_preset = new QComboBox();
    m_preset->setFixedWidth(290);
    for (const CanvasPreset &p : canvasPresets())
        m_preset->addItem(p.label, p.key);
    root->addWidget(m_preset);
    root->addSpacing(8);
    root->addWidget(new QLabel(QStringLiteral("캔버스 크기:")));
    auto *grid = new QGridLayout();
    grid->addWidget(new QLabel(QStringLiteral("가로 (Width)")), 0, 0);
    m_w = new QSpinBox();
    m_w->setRange(1, 20000);
    m_w->setFixedWidth(75);
    grid->addWidget(m_w, 1, 0);
    auto *swapBtn = new QToolButton();
    swapBtn->setIcon(icons::icon(QStringLiteral("swap")));
    swapBtn->setAutoRaise(true);
    swapBtn->setToolTip(QStringLiteral("가로/세로 바꾸기"));
    connect(swapBtn, &QToolButton::clicked, this, &NewPage::swap);
    grid->addWidget(swapBtn, 1, 1, Qt::AlignLeft);
    grid->addWidget(new QLabel(QStringLiteral("세로 (Height)")), 2, 0);
    m_h = new QSpinBox();
    m_h->setRange(1, 20000);
    m_h->setFixedWidth(75);
    grid->addWidget(m_h, 3, 0);
    grid->setColumnStretch(2, 1);
    root->addLayout(grid);
    root->addSpacing(8);
    root->addWidget(sectionTitle(QStringLiteral("배경색")));
    m_bgBtn = new QToolButton();
    m_bgBtn->setFixedSize(76, 54);
    connect(m_bgBtn, &QToolButton::clicked, this, &NewPage::pickBackground);
    root->addWidget(m_bgBtn);
    root->addStretch(1);

    m_w->setValue(size.width());
    m_h->setValue(size.height());
    m_preset->setCurrentIndex(std::max(0, m_preset->findData(preset)));
    connect(m_preset, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewPage::applyPreset);
    connect(m_w, qOverload<int>(&QSpinBox::valueChanged), this, &NewPage::manual);
    connect(m_h, qOverload<int>(&QSpinBox::valueChanged), this, &NewPage::manual);
    updateBackground();
    applyPreset();
}

QString NewPage::presetKey() const
{
    return m_preset->currentData().toString();
}

QSize NewPage::canvasSize() const
{
    return QSize(m_w->value(), m_h->value());
}

void NewPage::applyPreset()
{
    const QString key = presetKey();
    QSize size;
    if (key == QLatin1String("clipboard")) {
        const QImage img = QGuiApplication::clipboard()->image();
        if (!img.isNull())
            size = img.size();
    } else if (key == QLatin1String("screen")) {
        if (QScreen *screen = QGuiApplication::primaryScreen())
            size = screen->size();
    } else {
        for (const CanvasPreset &p : canvasPresets()) {
            if (p.key == key && p.size.isValid())
                size = p.size;
        }
    }
    if (size.isValid()) {
        const QSignalBlocker bw(m_w);
        const QSignalBlocker bh(m_h);
        m_w->setValue(size.width());
        m_h->setValue(size.height());
    }
    updateLabel();
}

void NewPage::manual()
{
    {
        const QSignalBlocker blocker(m_preset);
        m_preset->setCurrentIndex(m_preset->findData(QStringLiteral("custom")));
    }
    updateLabel();
}

void NewPage::swap()
{
    const int w = m_w->value();
    const int h = m_h->value();
    m_w->setValue(h);
    m_h->setValue(w);
}

void NewPage::updateLabel()
{
    m_sizeLabel->setText(QStringLiteral("캔버스 크기 : %1 x %2").arg(m_w->value()).arg(m_h->value()));
}

void NewPage::pickBackground()
{
    const QColor c = QColorDialog::getColor(m_bg, this, QStringLiteral("배경색"), QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        m_bg = c;
        updateBackground();
    }
}

void NewPage::updateBackground()
{
    m_bgBtn->setStyleSheet(
        QStringLiteral("background: %1; border: 1px solid #b0b0b0;").arg(m_bg.name(QColor::HexArgb)));
}

// ---------------------------------------------------------------- SharePage

SharePage::SharePage()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 30, 20);
    root->addWidget(pageTitle(QStringLiteral("공유")));
    root->addSpacing(14);
    struct Entry
    {
        QString key, icon, title, desc;
    };
    const QList<Entry> entries = {
        {QStringLiteral("clipboard"), QStringLiteral("copy"), QStringLiteral("클립보드로 복사"),
         QStringLiteral("이미지를 클립보드에 복사합니다.")},
        {QStringLiteral("email"), QStringLiteral("email"), QStringLiteral("이메일로 보내기"),
         QStringLiteral("기본 메일 프로그램으로 이미지를 첨부하여 보냅니다.")},
        {QStringLiteral("ftp"), QStringLiteral("ftp"), QStringLiteral("FTP 전송"),
         QStringLiteral("옵션에 설정된 FTP 서버로 이미지를 전송합니다.")},
        {QStringLiteral("program"), QStringLiteral("program"), QStringLiteral("외부 프로그램 연결"),
         QStringLiteral("옵션에 설정된 외부 프로그램으로 이미지를 엽니다.")},
        {QStringLiteral("default_app"), QStringLiteral("open"), QStringLiteral("기본 프로그램으로 열기"),
         QStringLiteral("시스템 기본 이미지 프로그램으로 엽니다.")},
    };
    for (const Entry &e : entries) {
        auto *btn = new BigButton(e.icon, e.title.section(QLatin1Char(' '), 0, 0));
        const QString key = e.key;
        connect(btn, &QToolButton::clicked, this, [this, key] { Q_EMIT share(key); });
        root->addLayout(bigButtonRow(btn, e.title, e.desc));
        root->addSpacing(6);
    }
    root->addStretch(1);
}

// ---------------------------------------------------------------- ThumbnailPage

ThumbnailPage::ThumbnailPage()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 30, 20);
    root->addWidget(pageTitle(QStringLiteral("썸네일")));
    root->addSpacing(14);
    m_area = new QScrollArea();
    m_area->setWidgetResizable(true);
    m_area->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_area, 1);
}

void ThumbnailPage::setDocuments(const QList<QPair<QString, QPixmap>> &docs)
{
    auto *host = new QWidget();
    auto *grid = new QGridLayout(host);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    for (int i = 0; i < docs.size(); ++i) {
        auto *btn = new QToolButton();
        btn->setObjectName(QStringLiteral("BigButton"));
        btn->setIcon(docs.at(i).second.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        btn->setIconSize(QSize(160, 120));
        btn->setText(docs.at(i).first);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(180, 160);
        connect(btn, &QToolButton::clicked, this, [this, i] { Q_EMIT activate(i); });
        grid->addWidget(btn, i / 4, i % 4);
    }
    if (docs.isEmpty())
        grid->addWidget(new QLabel(QStringLiteral("열려 있는 이미지가 없습니다.")), 0, 0);
    m_area->setWidget(host);
}

// ---------------------------------------------------------------- InfoPage

InfoPage::InfoPage()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 30, 30, 20);
    root->addWidget(pageTitle(QStringLiteral("정보")));
    root->addSpacing(14);
    auto *cols = new QHBoxLayout();
    auto *left = new QVBoxLayout();
    left->addWidget(sectionTitle(QStringLiteral("프로그램 정보")));
    auto *head = new QHBoxLayout();
    auto *logo = new QLabel();
    logo->setPixmap(icons::render(QStringLiteral("logo"), 64));
    head->addWidget(logo);
    auto *names = new QVBoxLayout();
    auto *n = new QLabel(QString::fromUtf8(kAppNameKo) + QStringLiteral(" (MikMick)"));
    QFont f;
    f.setPixelSize(18);
    n->setFont(f);
    names->addWidget(n);
    names->addWidget(new QLabel(QStringLiteral("버전 ") + QString::fromUtf8(kVersion)));
    names->addStretch(1);
    head->addLayout(names);
    head->addStretch(1);
    left->addLayout(head);
    auto *notice = new QLabel(QStringLiteral(
        "믹믹은 리눅스를 위한 무료 오픈소스 화면 캡처 및 이미지 편집 프로그램입니다. "
        "MIT 라이선스에 따라 누구나 자유롭게 사용, 수정, 배포할 수 있습니다. "
        "믹믹은 PicPick(NGWIN)과 제휴 관계가 없는 독립 프로젝트입니다."));
    notice->setWordWrap(true);
    notice->setObjectName(QStringLiteral("ItemDesc"));
    notice->setMaximumWidth(290);
    left->addWidget(notice);
    QFrame *line = hline();
    line->setFixedWidth(290);
    left->addWidget(line);

    auto *web = new BigButton(QStringLiteral("info"), QStringLiteral("공식 웹사이트"));
    connect(web, &QToolButton::clicked, this, &InfoPage::homepage);
    left->addLayout(bigButtonRow(web, QStringLiteral("공식 웹사이트"), QStringLiteral("믹믹 GitHub 저장소에 연결합니다.")));
    left->addSpacing(6);
    auto *upd = new BigButton(QStringLiteral("update"), QStringLiteral("업데이트 확인"));
    connect(upd, &QToolButton::clicked, this, &InfoPage::checkUpdate);
    left->addLayout(
        bigButtonRow(upd, QStringLiteral("업데이트 확인"), QStringLiteral("최신 버전 업데이트 여부를 확인합니다.")));
    left->addSpacing(6);
    left->addStretch(1);
    cols->addLayout(left, 1);
    cols->addWidget(vline());
    auto *right = new QVBoxLayout();
    right->addWidget(sectionTitle(QStringLiteral("라이선스 정보")));
    auto *lic = new QLabel(QStringLiteral("MIT License\n오픈소스 - 등록 불필요"));
    lic->setStyleSheet(QStringLiteral("font-size: 14px; color: #333;"));
    right->addWidget(lic);
    right->addStretch(1);
    cols->addLayout(right, 1);
    root->addLayout(cols, 1);
}

// ---------------------------------------------------------------- Backstage

const QStringList &Backstage::pageItems()
{
    static const QStringList items = {QStringLiteral("start"), QStringLiteral("new"), QStringLiteral("share"),
                                      QStringLiteral("thumbnail"), QStringLiteral("info")};
    return items;
}

Backstage::Backstage(bool hideStart, const QString &preset, const QSize &size, const QColor &bg)
{
    setObjectName(QStringLiteral("Backstage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(backstageQss());
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *side = new QWidget();
    side->setObjectName(QStringLiteral("BackstageSide"));
    side->setAttribute(Qt::WA_StyledBackground, true);
    side->setFixedWidth(150);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 18, 0, 18);
    sideLayout->setSpacing(0);
    auto *backBtn = new QToolButton();
    backBtn->setIcon(icons::icon(QStringLiteral("back")));
    backBtn->setIconSize(QSize(34, 34));
    connect(backBtn, &QToolButton::clicked, this, &Backstage::back);
    backBtn->setToolTip(QStringLiteral("돌아가기"));
    sideLayout->addWidget(backBtn, 0, Qt::AlignLeft);
    backBtn->setStyleSheet(QStringLiteral("margin-left: 16px;"));
    sideLayout->addSpacing(18);

    const QList<QPair<QString, QString>> sideItems = {
        {QStringLiteral("start"), QStringLiteral("시작")},
        {QStringLiteral("new"), QStringLiteral("새로 만들기")},
        {QStringLiteral("open"), QStringLiteral("열기")},
        {QStringLiteral("save"), QStringLiteral("저장")},
        {QStringLiteral("save_as"), QStringLiteral("다른 이름으로 저장")},
        {QStringLiteral("print"), QStringLiteral("인쇄")},
        {QStringLiteral("share"), QStringLiteral("공유")},
        {QStringLiteral("thumbnail"), QStringLiteral("썸네일")},
        {QStringLiteral("close"), QStringLiteral("닫기")},
        {QString(), QString()},
        {QStringLiteral("options"), QStringLiteral("옵션")},
        {QStringLiteral("info"), QStringLiteral("정보")},
    };
    for (const auto &[key, label] : sideItems) {
        if (key.isEmpty()) {
            sideLayout->addSpacing(36);
            continue;
        }
        auto *btn = new QPushButton(label);
        btn->setCheckable(pageItems().contains(key));
        btn->setAutoExclusive(false);
        const QString k = key;
        connect(btn, &QPushButton::clicked, this, [this, k] { onSide(k); });
        sideLayout->addWidget(btn);
        m_buttons.insert(key, btn);
    }
    sideLayout->addStretch(1);
    root->addWidget(side);

    m_stack = new QStackedWidget();
    m_startPage = new StartPage(hideStart);
    m_newPage = new NewPage(preset, size, bg);
    m_sharePage = new SharePage();
    m_thumbPage = new ThumbnailPage();
    m_infoPage = new InfoPage();
    m_pages.insert(QStringLiteral("start"), m_startPage);
    m_pages.insert(QStringLiteral("new"), m_newPage);
    m_pages.insert(QStringLiteral("share"), m_sharePage);
    m_pages.insert(QStringLiteral("thumbnail"), m_thumbPage);
    m_pages.insert(QStringLiteral("info"), m_infoPage);
    for (const QString &key : pageItems())
        m_stack->addWidget(m_pages.value(key));
    root->addWidget(m_stack, 1);

    connect(m_startPage, &StartPage::action, this, &Backstage::onStartAction);
    connect(m_newPage, &NewPage::createRequested, this, [this](const QSize &s, const QColor &c) {
        Q_EMIT command(QStringLiteral("create"),
                       QStringLiteral("%1x%2:%3").arg(s.width()).arg(s.height()).arg(c.name(QColor::HexArgb)));
    });
    connect(m_sharePage, &SharePage::share, this,
            [this](const QString &k) { Q_EMIT command(QStringLiteral("share"), k); });
    connect(m_thumbPage, &ThumbnailPage::activate, this,
            [this](int i) { Q_EMIT command(QStringLiteral("activate"), QString::number(i)); });
    connect(m_infoPage, &InfoPage::homepage, this, [this] { Q_EMIT command(QStringLiteral("homepage"), QString()); });
    connect(m_infoPage, &InfoPage::checkUpdate, this,
            [this] { Q_EMIT command(QStringLiteral("check_update"), QString()); });
    showPage(QStringLiteral("start"));
}

void Backstage::onStartAction(const QString &kind, const QString &name)
{
    if (kind == QLatin1String("new"))
        showPage(QStringLiteral("new"));
    else
        Q_EMIT command(kind, name);
}

void Backstage::onSide(const QString &key)
{
    if (pageItems().contains(key)) {
        showPage(key);
        if (key == QLatin1String("thumbnail"))
            Q_EMIT command(QStringLiteral("thumbnails"), QString());
    } else {
        m_buttons.value(key)->setChecked(false);
        Q_EMIT command(key, QString());
    }
}

void Backstage::showPage(const QString &key)
{
    const QString page = m_pages.contains(key) ? key : QStringLiteral("start");
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        if (it.value()->isCheckable())
            it.value()->setChecked(it.key() == page);
    }
    m_stack->setCurrentWidget(m_pages.value(page));
}

QString Backstage::currentPage() const
{
    return m_pages.key(m_stack->currentWidget());
}

void Backstage::setHasDocument(bool hasDoc)
{
    for (const char *key : {"save", "save_as", "print", "share", "thumbnail", "close"})
        m_buttons.value(QLatin1String(key))->setEnabled(hasDoc);
}

} // namespace mm::editor
