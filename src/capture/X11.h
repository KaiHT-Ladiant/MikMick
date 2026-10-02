#pragma once

#include "capture/Backend.h"

#include <QList>
#include <QRect>

#include <optional>

// XCB helpers. Everything degrades to "nothing available" when MIKMICK_HAVE_X11 is undefined
// or the Qt platform is not xcb.
namespace mm::capture::x11 {

bool available();
bool hasXTest();

std::optional<WindowInfo> activeWindow();
// Viewable top-level windows, topmost first, excluding this process.
QList<WindowInfo> windowList();

// XTest button press + release, repeated (button 4 = wheel up, 5 = wheel down).
bool clickButton(int button, int repeat = 1, int delayMs = 0);

// X11 device pixels -> Qt logical global coordinates.
QRect toLogical(const QRect &nativeRect);

} // namespace mm::capture::x11
