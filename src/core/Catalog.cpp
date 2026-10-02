#include "core/Catalog.h"

namespace mm {

const QList<CaptureModeInfo> &captureModes()
{
    static const QList<CaptureModeInfo> modes = {
        {QStringLiteral("fullscreen"), QStringLiteral("cap_fullscreen"), QStringLiteral("전체화면 캡처하기")},
        {QStringLiteral("active_window"), QStringLiteral("cap_window"), QStringLiteral("활성화된 윈도우 캡처")},
        {QStringLiteral("window_control"), QStringLiteral("cap_control"), QStringLiteral("윈도우 컨트롤 캡처")},
        {QStringLiteral("scroll"), QStringLiteral("cap_scroll"), QStringLiteral("자동 스크롤 캡처")},
        {QStringLiteral("region"), QStringLiteral("cap_region"), QStringLiteral("영역을 지정하여 캡처")},
        {QStringLiteral("fixed"), QStringLiteral("cap_fixed"), QStringLiteral("고정된 사각 영역 캡처")},
        {QStringLiteral("freehand"), QStringLiteral("cap_freehand"), QStringLiteral("내 마음대로 캡처하기")},
        {QStringLiteral("repeat_last"), QStringLiteral("cap_repeat"), QStringLiteral("마지막 캡처 영역 반복")},
    };
    return modes;
}

QStringList captureModeIds()
{
    QStringList ids;
    for (const auto &m : captureModes())
        ids << m.id;
    return ids;
}

QString captureModeLabel(const QString &id)
{
    for (const auto &m : captureModes()) {
        if (m.id == id)
            return m.label;
    }
    return QStringLiteral("캡처");
}

bool isCaptureMode(const QString &id)
{
    return captureModeIds().contains(id);
}

const QList<ToolInfo> &graphicTools()
{
    static const QList<ToolInfo> tools = {
        {QStringLiteral("color_picker"), QStringLiteral("tool_color_picker"), QStringLiteral("색상 추출 도구"),
         QStringLiteral("화면으로부터 색상코드를 추출합니다.")},
        {QStringLiteral("palette"), QStringLiteral("tool_palette"), QStringLiteral("색상 팔레트"),
         QStringLiteral("색상코드를 편집하거나 형식을 변경합니다.")},
        {QStringLiteral("magnifier"), QStringLiteral("tool_magnifier"), QStringLiteral("돋보기"),
         QStringLiteral("화면을 확대하는 돋보기를 표시합니다.")},
        {QStringLiteral("ruler"), QStringLiteral("tool_ruler"), QStringLiteral("눈금자"),
         QStringLiteral("화면의 객체를 픽셀이나 인치로 표시합니다.")},
        {QStringLiteral("crosshair"), QStringLiteral("tool_crosshair"), QStringLiteral("십자선"),
         QStringLiteral("화면의 좌표 및 상대좌표를 계산합니다.")},
        {QStringLiteral("protractor"), QStringLiteral("tool_protractor"), QStringLiteral("각도기"),
         QStringLiteral("화면의 기울기나 각도를 계산합니다.")},
        {QStringLiteral("whiteboard"), QStringLiteral("tool_whiteboard"), QStringLiteral("프리젠테이션 도구"),
         QStringLiteral("화면에 선이나 도형을 직접 그릴 수 있습니다.")},
    };
    return tools;
}

QStringList toolIds()
{
    QStringList ids{QStringLiteral("editor")};
    for (const auto &t : graphicTools())
        ids << t.id;
    return ids;
}

QString toolLabel(const QString &id)
{
    if (id == QLatin1String("editor"))
        return QStringLiteral("믹믹 에디터");
    for (const auto &t : graphicTools()) {
        if (t.id == id)
            return t.title;
    }
    return id;
}

bool isTool(const QString &id)
{
    return toolIds().contains(id);
}

const QList<QPair<QString, QString>> &resultChoices()
{
    static const QList<QPair<QString, QString>> choices = {
        {QStringLiteral("editor"), QStringLiteral("믹믹 에디터")},
        {QStringLiteral("clipboard"), QStringLiteral("클립보드")},
        {QStringLiteral("file"), QStringLiteral("파일로 저장")},
        {QStringLiteral("autosave"), QStringLiteral("자동 저장")},
        {QStringLiteral("ftp"), QStringLiteral("FTP 전송")},
        {QStringLiteral("program"), QStringLiteral("외부 프로그램 연결")},
    };
    return choices;
}

const QList<QPair<QString, QString>> &defaultHotkeys()
{
    static const QList<QPair<QString, QString>> keys = {
        {QStringLiteral("fullscreen"), QStringLiteral("Print")},
        {QStringLiteral("active_window"), QStringLiteral("Alt+Print")},
        {QStringLiteral("window_control"), QStringLiteral("Ctrl+Print")},
        {QStringLiteral("scroll"), QStringLiteral("Ctrl+Alt+Print")},
        {QStringLiteral("region"), QStringLiteral("Shift+Print")},
        {QStringLiteral("fixed"), QStringLiteral("Shift+Ctrl+Print")},
        {QStringLiteral("freehand"), QStringLiteral("Shift+Ctrl+Alt+Print")},
        {QStringLiteral("repeat_last"), QString()},
        {QStringLiteral("editor"), QString()},
        {QStringLiteral("color_picker"), QString()},
        {QStringLiteral("palette"), QString()},
        {QStringLiteral("magnifier"), QString()},
        {QStringLiteral("ruler"), QString()},
        {QStringLiteral("protractor"), QString()},
        {QStringLiteral("crosshair"), QString()},
        {QStringLiteral("whiteboard"), QString()},
    };
    return keys;
}

} // namespace mm
