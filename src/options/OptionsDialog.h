#pragma once

#include <QColor>
#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace mm {
class Config;
}

namespace mm::options {

// 옵션 대화상자: 일반 / 에디터 / 캡처 / 파일 이름 / 자동 저장 / 이미지 / FTP 설정 / 단축키.
// Accepting writes every page into the config and saves it.
class OptionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OptionsDialog(Config &config, QWidget *parent = nullptr);
    ~OptionsDialog() override;

    enum Page { General, Editor, Capture, FileName, AutoSave, Image, Ftp, Hotkeys };
    void setCurrentPage(int page);
    int currentPage() const;

private:
    struct HotkeyRow
    {
        QCheckBox *shift;
        QCheckBox *ctrl;
        QCheckBox *alt;
        QComboBox *key;
    };

    void buildGeneral(QVBoxLayout *lay);
    void buildEditor(QVBoxLayout *lay);
    void buildCapture(QVBoxLayout *lay);
    void buildFilename(QVBoxLayout *lay);
    void buildAutosave(QVBoxLayout *lay);
    void buildImage(QVBoxLayout *lay);
    void buildFtp(QVBoxLayout *lay);
    void buildHotkeys(QVBoxLayout *lay);

    void pickBackground();
    void refreshBackground();
    void updateFilenamePreview();
    void browseFolder();
    void browseProgram();
    void testFtp();
    void applyHotkeyPreset(const QString &kind);
    void storeFtp();
    void acceptOptions();

    Config &m_config;
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;

    QComboBox *m_startMode = nullptr;
    QCheckBox *m_autostart = nullptr;
    QComboBox *m_autostartMode = nullptr;
    QCheckBox *m_checkUpdates = nullptr;

    QList<QPair<QString, QCheckBox *>> m_editorChecks;
    QRadioButton *m_bgSolid = nullptr;
    QRadioButton *m_bgGrid = nullptr;
    QColor m_bgColor;
    QToolButton *m_bgButton = nullptr;

    QComboBox *m_result = nullptr;
    QList<QPair<QString, QCheckBox *>> m_captureChecks;
    QSpinBox *m_delay = nullptr;
    QSpinBox *m_scrollDelay = nullptr;
    QCheckBox *m_magnifier = nullptr;
    QSlider *m_zoom = nullptr;

    QComboBox *m_pattern = nullptr;
    QComboBox *m_format = nullptr;
    QLabel *m_filenamePreview = nullptr;

    QCheckBox *m_autosave = nullptr;
    QLineEdit *m_folder = nullptr;
    QComboBox *m_program = nullptr;
    QLineEdit *m_programArgs = nullptr;

    QComboBox *m_qualityPreset = nullptr;
    QSpinBox *m_quality = nullptr;

    QLineEdit *m_ftpServer = nullptr;
    QSpinBox *m_ftpPort = nullptr;
    QLineEdit *m_ftpPath = nullptr;
    QCheckBox *m_ftpPassive = nullptr;
    QLineEdit *m_ftpUser = nullptr;
    QLineEdit *m_ftpPassword = nullptr;
    QPushButton *m_ftpTestButton = nullptr;
    QCheckBox *m_ftpOpen = nullptr;
    QCheckBox *m_ftpCopy = nullptr;
    QLineEdit *m_ftpUrl = nullptr;
    QLineEdit *m_ftpStatus = nullptr;

    QList<QPair<QString, HotkeyRow>> m_hotkeyRows;
};

} // namespace mm::options
