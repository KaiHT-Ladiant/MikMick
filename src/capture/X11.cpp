#include "capture/X11.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>

#ifdef MIKMICK_HAVE_X11
#include <QByteArray>
#include <QHash>
#include <QtGui/qguiapplication_platform.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include <xcb/xcb.h>
#include <xcb/xtest.h>
#endif

namespace mm::capture::x11 {

QRect toLogical(const QRect &nativeRect)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return nativeRect;
    auto nativeGeometry = [](const QScreen *s) {
        const QRect g = s->geometry();
        return QRect(g.topLeft(), g.size() * s->devicePixelRatio());
    };
    QScreen *screen = nullptr;
    for (const QPoint &probe : {nativeRect.center(), nativeRect.topLeft()}) {
        for (QScreen *s : screens) {
            if (nativeGeometry(s).contains(probe)) {
                screen = s;
                break;
            }
        }
        if (screen)
            break;
    }
    if (!screen)
        screen = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen() : screens.first();
    const double dpr = screen->devicePixelRatio();
    if (qFuzzyCompare(dpr, 1.0))
        return nativeRect;
    // Qt keeps a screen's logical origin equal to its native origin.
    const QPoint origin = screen->geometry().topLeft();
    const double x = origin.x() + (nativeRect.x() - origin.x()) / dpr;
    const double y = origin.y() + (nativeRect.y() - origin.y()) / dpr;
    return QRect(qRound(x), qRound(y), qRound(nativeRect.width() / dpr), qRound(nativeRect.height() / dpr));
}

#ifdef MIKMICK_HAVE_X11

namespace {

template<typename T>
using Reply = std::unique_ptr<T, void (*)(void *)>;

template<typename T>
Reply<T> wrap(T *reply)
{
    return Reply<T>(reply, &std::free);
}

xcb_connection_t *connection()
{
#if QT_CONFIG(xcb)
    if (!qGuiApp || QGuiApplication::platformName() != QLatin1String("xcb"))
        return nullptr;
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    xcb_connection_t *c = x11 ? x11->connection() : nullptr;
    if (!c || xcb_connection_has_error(c))
        return nullptr;
    return c;
#else
    return nullptr;
#endif
}

xcb_window_t rootWindow(xcb_connection_t *c)
{
    const xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(c));
    return it.rem ? it.data->root : xcb_window_t(XCB_WINDOW_NONE);
}

xcb_atom_t atom(xcb_connection_t *c, const char *name)
{
    static QHash<QByteArray, xcb_atom_t> cache;
    const QByteArray key(name);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();
    auto reply = wrap(xcb_intern_atom_reply(c, xcb_intern_atom(c, 1, uint16_t(key.size()), name), nullptr));
    const xcb_atom_t value = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    if (value != XCB_ATOM_NONE)
        cache.insert(key, value);
    return value;
}

struct Property
{
    xcb_atom_t type = XCB_ATOM_NONE;
    int format = 0;
    QByteArray data;

    QList<quint32> values32() const
    {
        QList<quint32> out;
        if (format != 32)
            return out;
        for (int i = 0; i + 4 <= data.size(); i += 4) {
            quint32 v = 0;
            std::memcpy(&v, data.constData() + i, 4);
            out << v;
        }
        return out;
    }
};

std::optional<Property> property(xcb_connection_t *c, xcb_window_t window, xcb_atom_t prop,
                                 xcb_atom_t type = XCB_GET_PROPERTY_TYPE_ANY)
{
    if (prop == XCB_ATOM_NONE)
        return std::nullopt;
    auto reply = wrap(xcb_get_property_reply(c, xcb_get_property(c, 0, window, prop, type, 0, 1 << 20), nullptr));
    if (!reply || reply->type == XCB_ATOM_NONE)
        return std::nullopt;
    Property p;
    p.type = reply->type;
    p.format = reply->format;
    p.data = QByteArray(static_cast<const char *>(xcb_get_property_value(reply.get())),
                        xcb_get_property_value_length(reply.get()));
    return p;
}

QList<quint32> cardinals(xcb_connection_t *c, xcb_window_t window, const char *name)
{
    const auto p = property(c, window, atom(c, name));
    return p ? p->values32() : QList<quint32>();
}

qint64 windowPid(xcb_connection_t *c, xcb_window_t window)
{
    const QList<quint32> v = cardinals(c, window, "_NET_WM_PID");
    return v.isEmpty() ? -1 : qint64(v.first());
}

QString windowTitle(xcb_connection_t *c, xcb_window_t window)
{
    const xcb_atom_t utf8 = atom(c, "UTF8_STRING");
    if (const auto p = property(c, window, atom(c, "_NET_WM_NAME")); p && !p->data.isEmpty())
        return QString::fromUtf8(p->data);
    if (const auto p = property(c, window, XCB_ATOM_WM_NAME)) {
        if (p->type == utf8)
            return QString::fromUtf8(p->data);
        return QString::fromLatin1(p->data);
    }
    return {};
}

// Root-relative rectangle in device pixels, grown by the WM frame and shrunk by client-side shadows.
std::optional<QRect> frameRect(xcb_connection_t *c, xcb_window_t root, xcb_window_t window)
{
    auto geo = wrap(xcb_get_geometry_reply(c, xcb_get_geometry(c, window), nullptr));
    auto tr = wrap(xcb_translate_coordinates_reply(c, xcb_translate_coordinates(c, window, root, 0, 0), nullptr));
    if (!geo || !tr)
        return std::nullopt;
    const int border = geo->border_width;
    QRect r(tr->dst_x - border, tr->dst_y - border, geo->width + 2 * border, geo->height + 2 * border);
    const QList<quint32> frame = cardinals(c, window, "_NET_FRAME_EXTENTS");
    if (frame.size() >= 4)
        r.adjust(-int(frame[0]), -int(frame[2]), int(frame[1]), int(frame[3]));
    const QList<quint32> shadow = cardinals(c, window, "_GTK_FRAME_EXTENTS");
    if (shadow.size() >= 4)
        r.adjust(int(shadow[0]), int(shadow[2]), -int(shadow[1]), -int(shadow[3]));
    return r;
}

std::optional<WindowInfo> describe(xcb_connection_t *c, xcb_window_t root, xcb_window_t window)
{
    const auto native = frameRect(c, root, window);
    if (!native)
        return std::nullopt;
    WindowInfo info;
    info.rect = toLogical(*native);
    info.title = windowTitle(c, window);
    info.pid = windowPid(c, window);
    info.id = window;
    return info;
}

// Top-level ancestor (direct child of root) of a window, for WM-less sessions.
xcb_window_t topLevel(xcb_connection_t *c, xcb_window_t root, xcb_window_t window)
{
    for (int depth = 0; depth < 64 && window != XCB_WINDOW_NONE && window != root; ++depth) {
        auto tree = wrap(xcb_query_tree_reply(c, xcb_query_tree(c, window), nullptr));
        if (!tree)
            return XCB_WINDOW_NONE;
        if (tree->parent == root)
            return window;
        window = tree->parent;
    }
    return XCB_WINDOW_NONE;
}

} // namespace

bool available()
{
    return connection() != nullptr;
}

bool hasXTest()
{
    xcb_connection_t *c = connection();
    if (!c)
        return false;
    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(c, &xcb_test_id);
    return ext && ext->present;
}

std::optional<WindowInfo> activeWindow()
{
    xcb_connection_t *c = connection();
    if (!c)
        return std::nullopt;
    const xcb_window_t root = rootWindow(c);
    xcb_window_t window = XCB_WINDOW_NONE;
    if (const auto p = property(c, root, atom(c, "_NET_ACTIVE_WINDOW"), XCB_ATOM_WINDOW)) {
        const QList<quint32> v = p->values32();
        if (!v.isEmpty())
            window = v.first();
    } else {
        auto focus = wrap(xcb_get_input_focus_reply(c, xcb_get_input_focus(c), nullptr));
        if (focus && focus->focus != XCB_INPUT_FOCUS_POINTER_ROOT)
            window = topLevel(c, root, focus->focus);
    }
    if (window == XCB_WINDOW_NONE || window == root)
        return std::nullopt;
    return describe(c, root, window);
}

QList<WindowInfo> windowList()
{
    QList<WindowInfo> result;
    xcb_connection_t *c = connection();
    if (!c)
        return result;
    const xcb_window_t root = rootWindow(c);
    QList<quint32> ids;
    bool managed = true;
    if (const auto p = property(c, root, atom(c, "_NET_CLIENT_LIST_STACKING"), XCB_ATOM_WINDOW)) {
        ids = p->values32();
    } else {
        managed = false;
        auto tree = wrap(xcb_query_tree_reply(c, xcb_query_tree(c, root), nullptr));
        if (tree) {
            const xcb_window_t *children = xcb_query_tree_children(tree.get());
            const int n = xcb_query_tree_children_length(tree.get());
            for (int i = 0; i < n; ++i)
                ids << children[i];
        }
    }
    const qint64 ownPid = QCoreApplication::applicationPid();
    for (auto it = ids.crbegin(); it != ids.crend(); ++it) {
        const xcb_window_t window = *it;
        auto attrs = wrap(xcb_get_window_attributes_reply(c, xcb_get_window_attributes(c, window), nullptr));
        if (!attrs || attrs->map_state != XCB_MAP_STATE_VIEWABLE)
            continue;
        if (!managed && (attrs->override_redirect || attrs->_class == XCB_WINDOW_CLASS_INPUT_ONLY))
            continue;
        if (windowPid(c, window) == ownPid)
            continue;
        const auto info = describe(c, root, window);
        if (!info || info->rect.width() <= 1 || info->rect.height() <= 1)
            continue;
        result << *info;
    }
    return result;
}

bool clickButton(int button, int repeat, int delayMs)
{
    xcb_connection_t *c = connection();
    if (!c || !hasXTest())
        return false;
    for (int i = 0; i < repeat; ++i) {
        if (i > 0 && delayMs > 0)
            QThread::msleep(ulong(delayMs));
        xcb_test_fake_input(c, XCB_BUTTON_PRESS, uint8_t(button), XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_test_fake_input(c, XCB_BUTTON_RELEASE, uint8_t(button), XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_flush(c);
    }
    return true;
}

#else

bool available()
{
    return false;
}

bool hasXTest()
{
    return false;
}

std::optional<WindowInfo> activeWindow()
{
    return std::nullopt;
}

QList<WindowInfo> windowList()
{
    return {};
}

bool clickButton(int, int, int)
{
    return false;
}

#endif

} // namespace mm::capture::x11
