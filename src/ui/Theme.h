#pragma once

#include <QPalette>

namespace mm::theme {

// MikMick's windows are designed light (like PicPick) and several surfaces are painted white
// by style sheets, so the desktop palette (e.g. qt6ct "Kali-Dark") must not leak in.
QPalette lightPalette();

// Sets lightPalette() as the application palette. Platform themes such as qt6ct leave an
// explicitly set application palette alone.
void applyLightPalette();

} // namespace mm::theme
