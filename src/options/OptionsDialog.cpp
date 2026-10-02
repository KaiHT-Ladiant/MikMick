#include "options/OptionsDialog.h"

#include "core/Catalog.h"
#include "core/Config.h"
#include "core/Filename.h"
#include "core/Outputs.h"
#include "options/HotkeyText.h"
#include "ui/Icons.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

namespace mm::options {

namespace {

const QString kGeneral = QStringLiteral("general");
const QString kEditor = QStringLiteral("editor");
const QString kCapture = QStringLiteral("capture");
const QString kFilename = QStringLiteral("filename");
const QString kAutosave = QStringLiteral("autosave");
const QString kImage = QStringLiteral("image");
const QString kFtp = QStringLiteral("ftp");
const QString kHotkeys = QStringLiteral("hotkeys");

const char kOptionsQss[] = R"(
QListWidget#OptNav { background: white; border: 1px solid #c8c8c8; font-size: 13px; outline: 0; }
QListWidget#OptNav::item { padding: 7px; }
QListWidget#OptNav::item:selected { background: #cfcfcf; color: black; border: 1px solid #a8a8a8; }
QFrame#OptBody { background: white; border: 1px solid #c8c8c8; }
QLabel#OptHeader { background: #ebebeb; padding: 5px 8px; font-weight: bold; color: #444; }
)";

using Choice = QPair<QString, QString>;

const QList<Choice> &hotkeyRowsCapture()
{
    static const QList<Choice> rows = {
        {QStringLiteral("fullscreen"), QStringLiteral("전체화면 캡처하기")},
        {QStringLiteral("active_window"), QStringLiteral("활성화된 윈도우 캡처하기")},
        {QStringLiteral("window_control"), QStringLiteral("윈도우 컨트롤 캡처하기")},
        {QStringLiteral("scroll"), QStringLiteral("자동 스크롤 캡처하기")},
        {QStringLiteral("region"), QStringLiteral("영역을 지정하여 캡처하기")},
        {QStringLiteral("fixed"), QStringLiteral("고정된 사각 영역을 캡처하기")},
        {QStringLiteral("freehand"), QStringLiteral("내 마음대로 캡처하기")},
        {QStringLiteral("repeat_last"), QStringLiteral("마지막 캡처 영역 반복")},
    };
    return rows;
}

const QList<Choice> &hotkeyRowsTools()
{
    static const QList<Choice> rows = {
        {QStringLiteral("editor"), QStringLiteral("믹믹 에디터")},
        {QStringLiteral("color_picker"), QStringLiteral("색상 추출 도구")},
        {QStringLiteral("palette"), QStringLiteral("색상 팔레트")},
        {QStringLiteral("magnifier"), QStringLiteral("돋보기")},
        {QStringLiteral("ruler"), QStringLiteral("눈금자")},
        {QStringLiteral("protractor"), QStringLiteral("각도기")},
        {QStringLiteral("crosshair"), QStringLiteral("십자선")},
        {QStringLiteral("whiteboard"), QStringLiteral("프리젠테이션 도구")},
    };
    return rows;
}

const QList<Choice> &keyChoices()
{
    static const QList<Choice> keys = [] {
        QList<Choice> out{{QString(), QStringLiteral("없음")}, {QStringLiteral("Print"), QStringLiteral("PrintScreen")}};
        for (int i = 1; i <= 12; ++i)
            out << Choice(QStringLiteral("F%1").arg(i), QStringLiteral("F%1").arg(i));
        for (char c = 'A'; c <= 'Z'; ++c)
            out << Choice(QString(QLatin1Char(c)), QString(QLatin1Char(c)));
        for (char c = '0'; c <= '9'; ++c)
            out << Choice(QString(QLatin1Char(c)), QString(QLatin1Char(c)));
        return out;
    }();
    return keys;
}

QLabel *header(const QString &text)
{
    auto *lab = new QLabel(text);
    lab->setObjectName(QStringLiteral("OptHeader"));
    return lab;
}

void selectData(QComboBox *combo, const QVariant &data)
{
    combo->setCurrentIndex(qMax(0, combo->findData(data)));
}

QHBoxLayout *indented(QWidget *widget, int indent = 16)
{
    auto *row = new QHBoxLayout;
    row->addSpacing(indent);
    row->addWidget(widget);
    row->addStretch(1);
    return row;
}

QString firstLine(const QString &text)
{
    return text.section(QLatin1Char('\n'), 0, 0).trimmed();
}

// 시작 모드 미리보기 그림.
class StartPreview : public QWidget
{
public:
    StartPreview()
    {
        setFixedSize(230, 120);
    }

    void setMode(const QString &mode)
    {
        m_mode = mode;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(QStringLiteral("#0f2a4f")));
        if (m_mode == QLatin1String("editor")) {
            const QRectF win(55, 12, 120, 90);
            p.fillRect(win, QColor(QStringLiteral("#f3f3f3")));
            p.fillRect(QRectF(win.x(), win.y(), 22, win.height()), QColor(QStringLiteral("#2b579a")));
            p.setPen(QColor(QStringLiteral("#888")));
            for (int i = 0; i < 6; ++i) {
                const int y = int(win.y() + 18 + i * 11);
                p.drawLine(int(win.x() + 30), y, int(win.x() + 70), y);
                p.drawLine(int(win.x() + 78), y, int(win.x() + 112), y);
            }
        } else {
            p.fillRect(QRectF(0, 104, 230, 16), QColor(QStringLiteral("#1e1e1e")));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QStringLiteral("#ff5f6d")));
            p.drawEllipse(QRectF(200, 106, 12, 12));
        }
    }

private:
    QString m_mode = QStringLiteral("editor");
};

} // namespace

OptionsDialog::OptionsDialog(Config &config, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(QStringLiteral("옵션"));
    setWindowIcon(mm::icons::icon(QStringLiteral("options")));
    setStyleSheet(QString::fromUtf8(kOptionsQss));
    resize(640, 594);

    auto *root = new QVBoxLayout(this);
    auto *main = new QHBoxLayout;
    m_nav = new QListWidget;
    m_nav->setObjectName(QStringLiteral("OptNav"));
    m_nav->setFixedWidth(118);
    main->addWidget(m_nav);
    auto *body = new QFrame;
    body->setObjectName(QStringLiteral("OptBody"));
    auto *bodyLayout = new QVBoxLayout(body);
    m_pages = new QStackedWidget;
    bodyLayout->addWidget(m_pages);
    main->addWidget(body, 1);
    root->addLayout(main, 1);

    using Builder = void (OptionsDialog::*)(QVBoxLayout *);
    const QList<QPair<QString, Builder>> pages = {
        {QStringLiteral("일반"), &OptionsDialog::buildGeneral},
        {QStringLiteral("에디터"), &OptionsDialog::buildEditor},
        {QStringLiteral("캡처"), &OptionsDialog::buildCapture},
        {QStringLiteral("파일 이름"), &OptionsDialog::buildFilename},
        {QStringLiteral("자동 저장"), &OptionsDialog::buildAutosave},
        {QStringLiteral("이미지"), &OptionsDialog::buildImage},
        {QStringLiteral("FTP 설정"), &OptionsDialog::buildFtp},
        {QStringLiteral("단축키"), &OptionsDialog::buildHotkeys},
    };
    for (const auto &page : pages) {
        auto *item = new QListWidgetItem(page.first, m_nav);
        item->setTextAlignment(Qt::AlignCenter);
        auto *widget = new QWidget;
        auto *lay = new QVBoxLayout(widget);
        lay->setContentsMargins(8, 8, 8, 8);
        (this->*page.second)(lay);
        lay->addStretch(1);
        m_pages->addWidget(widget);
    }
    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *ok = new QPushButton(QStringLiteral("확인(&O)"));
    ok->setFixedWidth(92);
    connect(ok, &QPushButton::clicked, this, &OptionsDialog::acceptOptions);
    auto *cancel = new QPushButton(QStringLiteral("취소(&C)"));
    cancel->setFixedWidth(92);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    root->addLayout(buttons);
}

OptionsDialog::~OptionsDialog() = default;

void OptionsDialog::setCurrentPage(int page)
{
    m_nav->setCurrentRow(page);
}

int OptionsDialog::currentPage() const
{
    return m_nav->currentRow();
}

// ------------------------------------------------------------------------------------ pages
void OptionsDialog::buildGeneral(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("프로그램 시작 설정")));
    lay->addWidget(new QLabel(QStringLiteral("프로그램 시작 모드를 선택하세요 :")));
    m_startMode = new QComboBox;
    m_startMode->addItem(QStringLiteral("믹믹 에디터"), QStringLiteral("editor"));
    m_startMode->addItem(QStringLiteral("알림 영역에만 표시"), QStringLiteral("tray"));
    selectData(m_startMode, m_config.str(kGeneral, QStringLiteral("start_mode"), QStringLiteral("editor")));
    m_startMode->setFixedWidth(240);
    lay->addLayout(indented(m_startMode));

    auto *preview = new StartPreview;
    preview->setMode(m_startMode->currentData().toString());
    connect(m_startMode, qOverload<int>(&QComboBox::currentIndexChanged), preview,
            [this, preview]() { preview->setMode(m_startMode->currentData().toString()); });
    lay->addLayout(indented(preview));
    lay->addSpacing(8);

    m_autostart = new QCheckBox(QStringLiteral("로그인 시 자동 실행(&R)"));
    m_autostart->setChecked(m_config.flag(kGeneral, QStringLiteral("autostart"), false));
    lay->addWidget(m_autostart);
    m_autostartMode = new QComboBox;
    m_autostartMode->addItem(QStringLiteral("알림 영역에만 표시"), QStringLiteral("tray"));
    m_autostartMode->addItem(QStringLiteral("믹믹 에디터"), QStringLiteral("editor"));
    selectData(m_autostartMode, m_config.str(kGeneral, QStringLiteral("autostart_mode"), QStringLiteral("tray")));
    m_autostartMode->setFixedWidth(240);
    m_autostartMode->setEnabled(m_autostart->isChecked());
    connect(m_autostart, &QCheckBox::toggled, m_autostartMode, &QWidget::setEnabled);
    lay->addLayout(indented(m_autostartMode));

    m_checkUpdates = new QCheckBox(QStringLiteral("주기적으로 프로그램 업데이트를 확인합니다."));
    m_checkUpdates->setChecked(m_config.flag(kGeneral, QStringLiteral("check_updates"), true));
    lay->addWidget(m_checkUpdates);
}

void OptionsDialog::buildEditor(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("믹믹 에디터 설정")));
    const QList<Choice> checks = {
        {QStringLiteral("hide_on_capture"), QStringLiteral("캡처시 믹믹 에디터를 숨기기")},
        {QStringLiteral("center_image"), QStringLiteral("이미지를 화면 가운데에 표시")},
        {QStringLiteral("auto_shape_selection"), QStringLiteral("영역 선택시 자동으로 도형을 배경화하기")},
        {QStringLiteral("no_save_prompt"), QStringLiteral("이미지를 닫을 때 저장 여부를 확인하지 않기")},
        {QStringLiteral("quit_on_close"), QStringLiteral("믹믹 에디터를 닫을 때 프로그램을 종료하기")},
    };
    for (const auto &c : checks) {
        auto *chk = new QCheckBox(c.second);
        chk->setChecked(m_config.flag(kEditor, c.first, false));
        lay->addWidget(chk);
        m_editorChecks << qMakePair(c.first, chk);
    }
    lay->addSpacing(10);
    lay->addWidget(header(QStringLiteral("이미지 배경색")));

    auto *grid = new QGridLayout;
    m_bgSolid = new QRadioButton(QStringLiteral("단색"));
    m_bgGrid = new QRadioButton(QStringLiteral("그리드"));
    auto *group = new QButtonGroup(this);
    group->addButton(m_bgSolid);
    group->addButton(m_bgGrid);
    (m_config.str(kEditor, QStringLiteral("bg_mode")) == QLatin1String("grid") ? m_bgGrid : m_bgSolid)
        ->setChecked(true);
    m_bgColor = QColor(m_config.str(kEditor, QStringLiteral("bg_color"), QStringLiteral("#dcdcdc")));
    m_bgButton = new QToolButton;
    m_bgButton->setFixedSize(58, 24);
    connect(m_bgButton, &QToolButton::clicked, this, &OptionsDialog::pickBackground);
    refreshBackground();
    auto *gridSample = new QLabel;
    gridSample->setFixedSize(58, 24);
    gridSample->setStyleSheet(QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #ddd, stop:0.49 #ddd, stop:0.5 #fff, stop:1 #fff);"
        "border: 1px solid #aaa;"));
    grid->addWidget(m_bgSolid, 0, 0);
    grid->addWidget(m_bgButton, 0, 1);
    grid->addWidget(m_bgGrid, 1, 0);
    grid->addWidget(gridSample, 1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnMinimumWidth(0, 140);
    lay->addLayout(grid);
}

void OptionsDialog::pickBackground()
{
    const QColor c = QColorDialog::getColor(m_bgColor, this, QStringLiteral("배경색"));
    if (c.isValid()) {
        m_bgColor = c;
        refreshBackground();
    }
}

void OptionsDialog::refreshBackground()
{
    m_bgButton->setStyleSheet(QStringLiteral("background: %1; border: 1px solid #aaa;").arg(m_bgColor.name()));
}

void OptionsDialog::buildCapture(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("캡처 결과")));
    auto *lab = new QLabel(
        QStringLiteral("알림 영역의 아이콘 메뉴를 선택하거나 단축키를 사용할 경우 캡처 결과의 방식을 변경할 수 있습니다."));
    lab->setWordWrap(true);
    lay->addWidget(lab);
    m_result = new QComboBox;
    for (const auto &choice : resultChoices())
        m_result->addItem(choice.second, choice.first);
    selectData(m_result, m_config.str(kCapture, QStringLiteral("result"), QStringLiteral("editor")));
    m_result->setFixedWidth(250);
    lay->addWidget(m_result);
    lay->addSpacing(6);

    lay->addWidget(header(QStringLiteral("캡처 옵션")));
    const QList<Choice> checks = {
        {QStringLiteral("multi_monitor"), QStringLiteral("다중 모니터 기능 지원")},
        {QStringLiteral("sound"), QStringLiteral("캡처 완료시 효과음을 출력")},
        {QStringLiteral("include_cursor"), QStringLiteral("마우스 커서를 캡처된 이미지에 포함")},
        {QStringLiteral("always_clipboard"), QStringLiteral("캡처된 이미지를 항상 클립보드에 저장")},
        {QStringLiteral("show_toolbar"), QStringLiteral("캡처시 캡처도구모음(안내) 표시")},
    };
    for (const auto &c : checks) {
        auto *chk = new QCheckBox(c.second);
        chk->setChecked(m_config.flag(kCapture, c.first, false));
        lay->addWidget(chk);
        m_captureChecks << qMakePair(c.first, chk);
    }
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("캡처 지연 시간")));
    m_delay = new QSpinBox;
    m_delay->setRange(0, 60000);
    m_delay->setSingleStep(500);
    m_delay->setSuffix(QStringLiteral(" ms"));
    m_delay->setValue(m_config.num(kCapture, QStringLiteral("delay_ms"), 0));
    row->addWidget(m_delay);
    row->addStretch(1);
    lay->addLayout(row);
    lay->addSpacing(6);

    lay->addWidget(header(QStringLiteral("스크롤 캡처")));
    row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("스크롤 지연시간 :")));
    m_scrollDelay = new QSpinBox;
    m_scrollDelay->setRange(30, 5000);
    m_scrollDelay->setSingleStep(50);
    m_scrollDelay->setSuffix(QStringLiteral(" ms"));
    m_scrollDelay->setValue(m_config.num(kCapture, QStringLiteral("scroll_delay_ms"), 100));
    row->addWidget(m_scrollDelay);
    row->addStretch(1);
    lay->addLayout(row);
    lay->addSpacing(6);

    lay->addWidget(header(QStringLiteral("화면 확대창")));
    m_magnifier = new QCheckBox(QStringLiteral("캡처시 화면 확대창 표시"));
    m_magnifier->setChecked(m_config.flag(kCapture, QStringLiteral("magnifier"), true));
    lay->addWidget(m_magnifier);
    row = new QHBoxLayout;
    auto *minus = new QToolButton;
    minus->setText(QStringLiteral("−"));
    auto *plus = new QToolButton;
    plus->setText(QStringLiteral("+"));
    m_zoom = new QSlider(Qt::Horizontal);
    m_zoom->setRange(2, 12);
    m_zoom->setValue(m_config.num(kCapture, QStringLiteral("magnifier_zoom"), 6));
    m_zoom->setFixedWidth(130);
    auto *zoomLabel = new QLabel;
    const auto zoomText = [](int v) { return QStringLiteral("화면 확대 배율 : %1X").arg(v); };
    connect(m_zoom, &QSlider::valueChanged, zoomLabel, [zoomLabel, zoomText](int v) { zoomLabel->setText(zoomText(v)); });
    zoomLabel->setText(zoomText(m_zoom->value()));
    connect(minus, &QToolButton::clicked, m_zoom, [this]() { m_zoom->setValue(m_zoom->value() - 1); });
    connect(plus, &QToolButton::clicked, m_zoom, [this]() { m_zoom->setValue(m_zoom->value() + 1); });
    row->addSpacing(16);
    row->addWidget(minus);
    row->addWidget(m_zoom);
    row->addWidget(plus);
    row->addSpacing(10);
    row->addWidget(zoomLabel);
    row->addStretch(1);
    lay->addLayout(row);
}

void OptionsDialog::buildFilename(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("파일 이름")));
    lay->addWidget(new QLabel(QStringLiteral("자동 저장 기능이나 FTP 전송시 생성할 파일명을 입력하여 주십시오.")));
    m_pattern = new QComboBox;
    m_pattern->setEditable(true);
    for (const char *pat : {"%c", "%y-%m-%d_%h%n%s", "screenshot_%y%m%d_%c", "%u_%t"})
        m_pattern->addItem(QString::fromLatin1(pat));
    m_pattern->setEditText(m_config.str(kFilename, QStringLiteral("pattern"), QStringLiteral("%c")));
    lay->addWidget(m_pattern);
    lay->addSpacing(6);
    auto *example = new QLabel(QStringLiteral("예시)    %y : 년, %m : 월, %d : 일\n"
                                              "            %h : 시, %n : 분, %s : 초\n"
                                              "            %c : 번호\n"
                                              "            %u : 사용자 이름, %w : 컴퓨터 이름\n"
                                              "            %t : 유닉스 타임스탬프"));
    example->setStyleSheet(QStringLiteral("color: #333; margin-left: 20px;"));
    lay->addWidget(example);
    lay->addSpacing(10);

    lay->addWidget(header(QStringLiteral("파일 형식")));
    lay->addWidget(new QLabel(QStringLiteral("파일 형식")));
    m_format = new QComboBox;
    for (const auto &fmt : imageFormats())
        m_format->addItem(fmt.second, fmt.first);
    selectData(m_format, m_config.str(kFilename, QStringLiteral("format"), QStringLiteral("png")));
    m_format->setFixedWidth(300);
    lay->addWidget(m_format);
    m_filenamePreview = new QLabel;
    m_filenamePreview->setAlignment(Qt::AlignCenter);
    m_filenamePreview->setStyleSheet(
        QStringLiteral("border: 1px solid #aaa; padding: 6px; color: #1a5fd0; font-weight: bold;"));
    lay->addWidget(m_filenamePreview);
    connect(m_pattern, &QComboBox::editTextChanged, this, &OptionsDialog::updateFilenamePreview);
    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged), this, &OptionsDialog::updateFilenamePreview);
    updateFilenamePreview();
}

void OptionsDialog::updateFilenamePreview()
{
    const QString name = expandPattern(m_pattern->currentText(), m_config.num(kFilename, QStringLiteral("counter"), 0));
    m_filenamePreview->setText(name + QLatin1Char('.') + m_format->currentData().toString());
}

void OptionsDialog::buildAutosave(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("자동 저장")));
    m_autosave = new QCheckBox(QStringLiteral("자동 저장 기능 사용"));
    m_autosave->setChecked(m_config.flag(kAutosave, QStringLiteral("enabled"), false));
    lay->addWidget(m_autosave);
    lay->addWidget(new QLabel(QStringLiteral("이미지를 자동으로 저장할 폴더를 선택하십시오.")));
    auto *row = new QHBoxLayout;
    m_folder = new QLineEdit(m_config.str(kAutosave, QStringLiteral("folder")));
    m_folder->setPlaceholderText(QStringLiteral("~/Pictures/MikMick"));
    auto *browse = new QToolButton;
    browse->setText(QStringLiteral("···"));
    connect(browse, &QToolButton::clicked, this, &OptionsDialog::browseFolder);
    row->addWidget(m_folder);
    row->addWidget(browse);
    lay->addLayout(row);
    lay->addSpacing(14);

    lay->addWidget(header(QStringLiteral("외부 프로그램 연결")));
    lay->addWidget(new QLabel(QStringLiteral("캡처 후 연결할 외부 프로그램을 선택하여 주십시오.")));
    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(QStringLiteral("프로그램 경로 :")), 0, 0, Qt::AlignRight);
    m_program = new QComboBox;
    m_program->setEditable(true);
    m_program->addItem(QString());
    for (const char *prog : {"gimp", "krita", "pinta", "inkscape", "eog", "gwenview", "xdg-open"}) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(prog));
        if (!path.isEmpty())
            m_program->addItem(path);
    }
    m_program->setEditText(m_config.str(kAutosave, QStringLiteral("program")));
    auto *programRow = new QHBoxLayout;
    programRow->addWidget(m_program, 1);
    auto *programBrowse = new QToolButton;
    programBrowse->setText(QStringLiteral("···"));
    connect(programBrowse, &QToolButton::clicked, this, &OptionsDialog::browseProgram);
    programRow->addWidget(programBrowse);
    grid->addLayout(programRow, 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("매개변수 :")), 1, 0, Qt::AlignRight);
    m_programArgs = new QLineEdit(m_config.str(kAutosave, QStringLiteral("program_args")));
    m_programArgs->setPlaceholderText(QStringLiteral("%f : 파일 경로 (생략 시 마지막 인자로 전달)"));
    grid->addWidget(m_programArgs, 1, 1);
    lay->addLayout(grid);
}

void OptionsDialog::browseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("폴더 선택"), m_folder->text());
    if (!dir.isEmpty())
        m_folder->setText(dir);
}

void OptionsDialog::browseProgram()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("프로그램 선택"), QStringLiteral("/usr/bin"));
    if (!path.isEmpty())
        m_program->setEditText(path);
}

void OptionsDialog::buildImage(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("JPEG 옵션")));
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("화질")));
    row->addSpacing(30);
    m_qualityPreset = new QComboBox;
    const QList<QPair<QString, int>> presets = {
        {QStringLiteral("최대"), 100}, {QStringLiteral("높음"), 90}, {QStringLiteral("보통"), 75},
        {QStringLiteral("낮음"), 50}, {QStringLiteral("사용자 정의"), -1},
    };
    for (const auto &preset : presets)
        m_qualityPreset->addItem(preset.first, preset.second);
    m_qualityPreset->setFixedWidth(110);
    m_quality = new QSpinBox;
    m_quality->setRange(1, 100);
    m_quality->setValue(m_config.num(kImage, QStringLiteral("jpeg_quality"), 100));
    row->addWidget(m_qualityPreset);
    row->addWidget(m_quality);
    row->addStretch(1);
    lay->addLayout(row);

    row = new QHBoxLayout;
    row->addSpacing(70);
    auto *minus = new QToolButton;
    minus->setText(QStringLiteral("−"));
    auto *plus = new QToolButton;
    plus->setText(QStringLiteral("+"));
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(1, 100);
    slider->setValue(m_quality->value());
    slider->setFixedWidth(180);
    connect(slider, &QSlider::valueChanged, m_quality, &QSpinBox::setValue);
    connect(m_quality, qOverload<int>(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    connect(minus, &QToolButton::clicked, m_quality, [this]() { m_quality->setValue(m_quality->value() - 1); });
    connect(plus, &QToolButton::clicked, m_quality, [this]() { m_quality->setValue(m_quality->value() + 1); });
    row->addWidget(minus);
    row->addWidget(slider);
    row->addWidget(plus);
    row->addStretch(1);
    lay->addLayout(row);

    const auto syncPreset = [this](int v) {
        const int idx = m_qualityPreset->findData(v);
        const QSignalBlocker block(m_qualityPreset);
        m_qualityPreset->setCurrentIndex(idx >= 0 ? idx : m_qualityPreset->count() - 1);
    };
    connect(m_quality, qOverload<int>(&QSpinBox::valueChanged), m_qualityPreset, syncPreset);
    connect(m_qualityPreset, qOverload<int>(&QComboBox::currentIndexChanged), m_quality, [this]() {
        const int v = m_qualityPreset->currentData().toInt();
        if (v > 0)
            m_quality->setValue(v);
    });
    syncPreset(m_quality->value());
}

void OptionsDialog::buildFtp(QVBoxLayout *lay)
{
    lay->addWidget(header(QStringLiteral("FTP 서버 설정")));
    auto *grid = new QGridLayout;
    m_ftpServer = new QLineEdit(m_config.str(kFtp, QStringLiteral("server")));
    m_ftpPort = new QSpinBox;
    m_ftpPort->setRange(1, 65535);
    m_ftpPort->setValue(m_config.num(kFtp, QStringLiteral("port"), 21));
    m_ftpPath = new QLineEdit(m_config.str(kFtp, QStringLiteral("path")));
    m_ftpPassive = new QCheckBox(QStringLiteral("수동 모드 사용"));
    m_ftpPassive->setChecked(m_config.flag(kFtp, QStringLiteral("passive"), false));
    m_ftpUser = new QLineEdit(m_config.str(kFtp, QStringLiteral("user"), QStringLiteral("anonymous")));
    m_ftpPassword = new QLineEdit(m_config.str(kFtp, QStringLiteral("password")));
    m_ftpPassword->setEchoMode(QLineEdit::Password);
    grid->addWidget(new QLabel(QStringLiteral("FTP 서버 :")), 0, 0, Qt::AlignRight);
    grid->addWidget(m_ftpServer, 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("포트 :")), 0, 2, Qt::AlignRight);
    grid->addWidget(m_ftpPort, 0, 3);
    grid->addWidget(new QLabel(QStringLiteral("경로 :")), 1, 0, Qt::AlignRight);
    grid->addWidget(m_ftpPath, 1, 1);
    grid->addWidget(m_ftpPassive, 2, 1);
    grid->setRowMinimumHeight(3, 12);
    grid->addWidget(new QLabel(QStringLiteral("사용자 이름 :")), 4, 0, Qt::AlignRight);
    grid->addWidget(m_ftpUser, 4, 1);
    grid->addWidget(new QLabel(QStringLiteral("비밀번호 :")), 5, 0, Qt::AlignRight);
    grid->addWidget(m_ftpPassword, 5, 1);
    m_ftpTestButton = new QPushButton(QStringLiteral("접속 테스트"));
    connect(m_ftpTestButton, &QPushButton::clicked, this, &OptionsDialog::testFtp);
    grid->addWidget(m_ftpTestButton, 5, 2, 1, 2);
    lay->addLayout(grid);
    lay->addSpacing(10);

    auto *box = new QFrame;
    box->setFrameShape(QFrame::StyledPanel);
    auto *boxLayout = new QVBoxLayout(box);
    m_ftpOpen = new QCheckBox(QStringLiteral("전송 후 웹브라우저로 URL 열기"));
    m_ftpOpen->setChecked(m_config.flag(kFtp, QStringLiteral("open_url"), false));
    m_ftpCopy = new QCheckBox(QStringLiteral("전송 후 클립보드에 URL 복사"));
    m_ftpCopy->setChecked(m_config.flag(kFtp, QStringLiteral("copy_url"), false));
    boxLayout->addWidget(m_ftpOpen);
    boxLayout->addWidget(m_ftpCopy);
    auto *row = new QHBoxLayout;
    row->addSpacing(40);
    row->addWidget(new QLabel(QStringLiteral("URL")));
    m_ftpUrl = new QLineEdit(m_config.str(kFtp, QStringLiteral("url")));
    m_ftpUrl->setPlaceholderText(QStringLiteral("https://example.com/screenshots"));
    row->addWidget(m_ftpUrl);
    boxLayout->addLayout(row);
    lay->addWidget(box);
    lay->addSpacing(10);
    m_ftpStatus = new QLineEdit;
    m_ftpStatus->setReadOnly(true);
    lay->addWidget(m_ftpStatus);
}

void OptionsDialog::testFtp()
{
    // ftpTest reads every setting synchronously, so the config can be restored right away.
    const QJsonObject snapshot = m_config.snapshot();
    storeFtp();
    m_ftpTestButton->setEnabled(false);
    m_ftpStatus->setText(QStringLiteral("접속 중..."));
    QPointer<QPushButton> button = m_ftpTestButton;
    QPointer<QLineEdit> status = m_ftpStatus;
    outputs::ftpTest(m_config, this, [button, status](bool ok, const QString &message) {
        if (button)
            button->setEnabled(true);
        if (status)
            status->setText((ok ? QStringLiteral("접속 성공: ") : QStringLiteral("접속 실패: ")) + firstLine(message));
    });
    m_config.restore(snapshot);
}

void OptionsDialog::buildHotkeys(QVBoxLayout *lay)
{
    const QJsonObject hotkeys = m_config.section(kHotkeys);
    const QList<QPair<QString, const QList<Choice> *>> groups = {
        {QStringLiteral("화면 캡처 도구"), &hotkeyRowsCapture()},
        {QStringLiteral("기타 도구"), &hotkeyRowsTools()},
    };
    for (const auto &group : groups) {
        lay->addWidget(header(group.first));
        auto *grid = new QGridLayout;
        grid->setVerticalSpacing(2);
        int r = 0;
        for (const auto &rowInfo : *group.second) {
            const HotkeyParts parts = parseHotkey(hotkeys.value(rowInfo.first).toString());
            grid->addWidget(new QLabel(rowInfo.second), r, 0);
            HotkeyRow row{new QCheckBox(QStringLiteral("Shift")), new QCheckBox(QStringLiteral("Ctrl")),
                          new QCheckBox(QStringLiteral("Alt")), new QComboBox};
            row.shift->setChecked(parts.shift);
            row.ctrl->setChecked(parts.ctrl);
            row.alt->setChecked(parts.alt);
            for (const auto &choice : keyChoices())
                row.key->addItem(choice.second, choice.first);
            selectData(row.key, parts.key);
            row.key->setMinimumWidth(row.key->fontMetrics().horizontalAdvance(QStringLiteral("PrintScreen")) + 40);
            const auto toggle = [row]() {
                const bool assigned = !row.key->currentData().toString().isEmpty();
                row.shift->setEnabled(assigned);
                row.ctrl->setEnabled(assigned);
                row.alt->setEnabled(assigned);
            };
            connect(row.key, qOverload<int>(&QComboBox::currentIndexChanged), row.key, toggle);
            toggle();
            grid->addWidget(row.shift, r, 1);
            grid->addWidget(row.ctrl, r, 2);
            grid->addWidget(row.alt, r, 3);
            grid->addWidget(row.key, r, 4);
            m_hotkeyRows << qMakePair(rowInfo.first, row);
            ++r;
        }
        grid->setColumnStretch(0, 1);
        lay->addLayout(grid);
    }
    auto *row = new QHBoxLayout;
    auto *preset = new QComboBox;
    preset->addItem(QStringLiteral("사용자 정의"), QStringLiteral("custom"));
    preset->addItem(QStringLiteral("기본값"), QStringLiteral("default"));
    preset->addItem(QStringLiteral("모두 해제"), QStringLiteral("none"));
    preset->setFixedWidth(140);
    connect(preset, qOverload<int>(&QComboBox::activated), this,
            [this, preset]() { applyHotkeyPreset(preset->currentData().toString()); });
    row->addWidget(preset);
    row->addStretch(1);
    lay->addLayout(row);
    auto *note = new QLabel(
        QStringLiteral("※ Wayland 세션에서는 전역 단축키를 데스크톱 설정에서 'mikmick --capture region' 등으로 등록하세요."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    lay->addWidget(note);
}

void OptionsDialog::applyHotkeyPreset(const QString &kind)
{
    if (kind == QLatin1String("custom"))
        return;
    const bool defaults = kind == QLatin1String("default");
    for (const auto &entry : std::as_const(m_hotkeyRows)) {
        QString seq;
        if (defaults) {
            for (const auto &kv : defaultHotkeys()) {
                if (kv.first == entry.first)
                    seq = kv.second;
            }
        }
        const HotkeyParts parts = parseHotkey(seq);
        const HotkeyRow &row = entry.second;
        row.shift->setChecked(parts.shift);
        row.ctrl->setChecked(parts.ctrl);
        row.alt->setChecked(parts.alt);
        selectData(row.key, parts.key);
    }
}

// ------------------------------------------------------------------------------------- save
void OptionsDialog::storeFtp()
{
    m_config.set(kFtp, QStringLiteral("server"), m_ftpServer->text().trimmed());
    m_config.set(kFtp, QStringLiteral("port"), m_ftpPort->value());
    m_config.set(kFtp, QStringLiteral("path"), m_ftpPath->text().trimmed());
    m_config.set(kFtp, QStringLiteral("passive"), m_ftpPassive->isChecked());
    m_config.set(kFtp, QStringLiteral("user"), m_ftpUser->text());
    m_config.set(kFtp, QStringLiteral("password"), m_ftpPassword->text());
    m_config.set(kFtp, QStringLiteral("open_url"), m_ftpOpen->isChecked());
    m_config.set(kFtp, QStringLiteral("copy_url"), m_ftpCopy->isChecked());
    m_config.set(kFtp, QStringLiteral("url"), m_ftpUrl->text().trimmed());
}

void OptionsDialog::acceptOptions()
{
    QStringList seen;
    QJsonObject hotkeys;
    for (const auto &entry : std::as_const(m_hotkeyRows)) {
        const HotkeyRow &row = entry.second;
        const QString seq = buildHotkey(row.shift->isChecked(), row.ctrl->isChecked(), row.alt->isChecked(),
                                        row.key->currentData().toString());
        if (!seq.isEmpty() && seen.contains(seq)) {
            QMessageBox::warning(this, QStringLiteral("옵션"), QStringLiteral("단축키 '%1' 가 중복되었습니다.").arg(seq));
            m_nav->setCurrentRow(Hotkeys);
            return;
        }
        if (!seq.isEmpty())
            seen << seq;
        hotkeys.insert(entry.first, seq);
    }

    Config &c = m_config;
    c.set(kGeneral, QStringLiteral("start_mode"), m_startMode->currentData().toString());
    c.set(kGeneral, QStringLiteral("autostart"), m_autostart->isChecked());
    c.set(kGeneral, QStringLiteral("autostart_mode"), m_autostartMode->currentData().toString());
    c.set(kGeneral, QStringLiteral("check_updates"), m_checkUpdates->isChecked());

    for (const auto &check : std::as_const(m_editorChecks))
        c.set(kEditor, check.first, check.second->isChecked());
    c.set(kEditor, QStringLiteral("bg_mode"), m_bgGrid->isChecked() ? QStringLiteral("grid") : QStringLiteral("solid"));
    c.set(kEditor, QStringLiteral("bg_color"), m_bgColor.name());

    c.set(kCapture, QStringLiteral("result"), m_result->currentData().toString());
    for (const auto &check : std::as_const(m_captureChecks))
        c.set(kCapture, check.first, check.second->isChecked());
    c.set(kCapture, QStringLiteral("delay_ms"), m_delay->value());
    c.set(kCapture, QStringLiteral("scroll_delay_ms"), m_scrollDelay->value());
    c.set(kCapture, QStringLiteral("magnifier"), m_magnifier->isChecked());
    c.set(kCapture, QStringLiteral("magnifier_zoom"), m_zoom->value());

    const QString pattern = m_pattern->currentText();
    c.set(kFilename, QStringLiteral("pattern"), pattern.isEmpty() ? QStringLiteral("%c") : pattern);
    c.set(kFilename, QStringLiteral("format"), m_format->currentData().toString());

    c.set(kAutosave, QStringLiteral("enabled"), m_autosave->isChecked());
    c.set(kAutosave, QStringLiteral("folder"), m_folder->text().trimmed());
    c.set(kAutosave, QStringLiteral("program"), m_program->currentText().trimmed());
    c.set(kAutosave, QStringLiteral("program_args"), m_programArgs->text().trimmed());

    c.set(kImage, QStringLiteral("jpeg_quality"), m_quality->value());
    storeFtp();

    QJsonObject section = c.section(kHotkeys);
    for (auto it = hotkeys.begin(); it != hotkeys.end(); ++it)
        section.insert(it.key(), it.value());
    c.setSection(kHotkeys, section);
    c.save();
    accept();
}

} // namespace mm::options
