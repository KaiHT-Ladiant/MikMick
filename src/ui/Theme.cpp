#include "ui/Theme.h"

#include <QApplication>
#include <QColor>

namespace mm::theme {

QPalette lightPalette()
{
    QPalette pal;
    const auto set = [&pal](QPalette::ColorRole role, const char *active, const char *disabled = nullptr) {
        const QColor color(QString::fromLatin1(active));
        pal.setColor(QPalette::Active, role, color);
        pal.setColor(QPalette::Inactive, role, color);
        pal.setColor(QPalette::Disabled, role, disabled ? QColor(QString::fromLatin1(disabled)) : color);
    };
    set(QPalette::Window, "#f0f0f0");
    set(QPalette::WindowText, "#000000", "#a0a0a0");
    set(QPalette::Base, "#ffffff", "#f0f0f0");
    set(QPalette::AlternateBase, "#f7f7f7");
    set(QPalette::ToolTipBase, "#ffffdc");
    set(QPalette::ToolTipText, "#000000");
    set(QPalette::PlaceholderText, "#808080");
    set(QPalette::Text, "#000000", "#a0a0a0");
    set(QPalette::Button, "#f0f0f0");
    set(QPalette::ButtonText, "#000000", "#a0a0a0");
    set(QPalette::BrightText, "#ffffff");
    set(QPalette::Light, "#ffffff");
    set(QPalette::Midlight, "#e3e3e3");
    set(QPalette::Mid, "#a0a0a0");
    set(QPalette::Dark, "#a0a0a0");
    set(QPalette::Shadow, "#696969");
    set(QPalette::Highlight, "#0078d7", "#c0c0c0");
    set(QPalette::HighlightedText, "#ffffff");
    set(QPalette::Link, "#0066cc");
    set(QPalette::LinkVisited, "#7a3cc8");
    return pal;
}

void applyLightPalette()
{
    QApplication::setPalette(lightPalette());
}

} // namespace mm::theme
