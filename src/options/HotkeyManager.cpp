#include "options/HotkeyManager.h"

#include "core/Catalog.h"
#include "core/System.h"
#include "options/HotkeyText.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QList>
#include <QPair>
#include <QStringList>

#include <functional>

#if defined(MIKMICK_HAVE_X11)
#include <QtGui/qtgui-config.h>
#if QT_CONFIG(xcb)
#define MM_X11_HOTKEYS 1
#endif
#endif

#ifdef MM_X11_HOTKEYS
#include <QAbstractNativeEventFilter>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <cstdlib>
#endif

#ifdef MIKMICK_HAVE_DBUS
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QRandomGenerator>
#include <QVariantMap>
#endif

namespace mm::options {

namespace {

using HotkeyList = QList<QPair<QString, QString>>; // action id, canonical sequence

QString waylandHelp()
{
    return QStringLiteral("Wayland 세션에서 전역 단축키 포털(org.freedesktop.portal.GlobalShortcuts)을 사용할 수 "
                          "없습니다. 데스크톱 환경의 단축키 설정에서 'mikmick --capture region' 등의 명령을 직접 "
                          "등록하세요.");
}

[[maybe_unused]] QString actionLabel(const QString &id)
{
    return isCaptureMode(id) ? captureModeLabel(id) : toolLabel(id);
}

} // namespace

// ------------------------------------------------------------------------------------- X11
#ifdef MM_X11_HOTKEYS
namespace {

constexpr xcb_keysym_t kKeysymPrint = 0xff61;
constexpr xcb_keysym_t kKeysymF1 = 0xffbe;
constexpr xcb_keysym_t kKeysymNumLock = 0xff7f;
constexpr xcb_keysym_t kKeysymSwitchVt1 = 0x1008fe01;
constexpr xcb_keysym_t kKeysymSwitchVt12 = 0x1008fe0c;
constexpr uint16_t kHotkeyMods = XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1;

xcb_keysym_t keysymFor(const QString &key)
{
    if (key == QLatin1String("Print"))
        return kKeysymPrint;
    if (key.size() >= 2 && key.at(0) == QLatin1Char('F')) {
        bool ok = false;
        const int n = QStringView(key).mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 35)
            return kKeysymF1 + xcb_keysym_t(n - 1);
        return XCB_NO_SYMBOL;
    }
    if (key.size() == 1) {
        const char16_t c = key.at(0).toLower().unicode();
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9'))
            return xcb_keysym_t(c);
    }
    return XCB_NO_SYMBOL;
}

class X11Grabber : public QAbstractNativeEventFilter
{
public:
    explicit X11Grabber(std::function<void(const QString &)> fire)
        : m_fire(std::move(fire))
    {
    }

    ~X11Grabber() override
    {
        ungrabAll();
        if (m_installed && QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(this);
    }

    static xcb_connection_t *connection()
    {
        if (!qGuiApp || QGuiApplication::platformName() != QLatin1String("xcb"))
            return nullptr;
        auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        return x11 ? x11->connection() : nullptr;
    }

    // Returns the sequences another client already owns; unmappable ones go to *unknown and
    // Ctrl+Alt combinations the server turns into a VT switch go to *vtSwitch.
    QStringList grab(const HotkeyList &hotkeys, QStringList *unknown, QStringList *vtSwitch)
    {
        ungrabAll();
        m_conn = connection();
        if (!m_conn)
            return {};
        if (!m_installed) {
            QCoreApplication::instance()->installNativeEventFilter(this);
            m_installed = true;
        }
        m_roots.clear();
        for (auto it = xcb_setup_roots_iterator(xcb_get_setup(m_conn)); it.rem; xcb_screen_next(&it))
            m_roots << it.data->root;

        xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(m_conn);
        if (!symbols)
            return {};
        m_numLock = numLockMask(symbols);
        const uint16_t locks[] = {0, XCB_MOD_MASK_LOCK, m_numLock, uint16_t(XCB_MOD_MASK_LOCK | m_numLock)};

        QStringList busy;
        for (const auto &hk : hotkeys) {
            const HotkeyParts parts = parseHotkey(hk.second);
            const xcb_keysym_t sym = keysymFor(parts.key);
            xcb_keycode_t *codes = sym != XCB_NO_SYMBOL ? xcb_key_symbols_get_keycode(symbols, sym) : nullptr;
            if (!codes || *codes == XCB_NO_SYMBOL) {
                std::free(codes);
                if (unknown)
                    *unknown << hk.second;
                continue;
            }
            uint16_t mods = 0;
            if (parts.shift)
                mods |= XCB_MOD_MASK_SHIFT;
            if (parts.ctrl)
                mods |= XCB_MOD_MASK_CONTROL;
            if (parts.alt)
                mods |= XCB_MOD_MASK_1;
            if (vtSwitch && parts.ctrl && parts.alt && isVtSwitchKey(symbols, codes))
                *vtSwitch << hk.second;

            QList<Grab> mine;
            QList<xcb_void_cookie_t> cookies;
            for (const xcb_keycode_t *code = codes; *code != XCB_NO_SYMBOL; ++code) {
                for (const xcb_window_t root : std::as_const(m_roots)) {
                    for (const uint16_t lock : locks) {
                        const uint16_t m = mods | lock;
                        cookies << xcb_grab_key_checked(m_conn, 0, root, m, *code, XCB_GRAB_MODE_ASYNC,
                                                        XCB_GRAB_MODE_ASYNC);
                        mine << Grab{root, *code, m, hk.first};
                    }
                }
            }
            std::free(codes);

            bool failed = false;
            for (const xcb_void_cookie_t &cookie : std::as_const(cookies)) {
                if (xcb_generic_error_t *err = xcb_request_check(m_conn, cookie)) {
                    failed = true;
                    std::free(err);
                }
            }
            if (failed) {
                ungrab(mine);
                busy << hk.second;
            } else {
                m_grabs << mine;
            }
        }
        xcb_key_symbols_free(symbols);
        xcb_flush(m_conn);
        return busy;
    }

    void ungrabAll()
    {
        if (m_conn && !m_grabs.isEmpty() && connection() == m_conn) {
            ungrab(m_grabs);
            xcb_flush(m_conn);
        }
        m_grabs.clear();
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (m_grabs.isEmpty() || eventType != "xcb_generic_event_t")
            return false;
        auto *ev = static_cast<xcb_generic_event_t *>(message);
        const uint8_t type = ev->response_type & 0x7f;
        if (type != XCB_KEY_PRESS && type != XCB_KEY_RELEASE)
            return false;
        // xcb_key_release_event_t is a typedef of the press event.
        auto *key = reinterpret_cast<xcb_key_press_event_t *>(ev);
        const uint16_t state = key->state & (kHotkeyMods | XCB_MOD_MASK_LOCK | m_numLock);
        for (const Grab &g : std::as_const(m_grabs)) {
            if (g.keycode != key->detail || g.mods != state || g.root != key->event)
                continue;
            if (type == XCB_KEY_RELEASE) {
                m_releasedKey = key->detail;
                m_releaseTime = key->time;
                return true;
            }
            // Auto-repeat sends a release and a press with the same timestamp.
            const bool repeat = key->detail == m_releasedKey && key->time == m_releaseTime;
            if (!repeat)
                m_fire(g.action);
            return true;
        }
        return false;
    }

private:
    struct Grab
    {
        xcb_window_t root;
        xcb_keycode_t keycode;
        uint16_t mods;
        QString action;
    };

    void ungrab(const QList<Grab> &grabs)
    {
        for (const Grab &g : grabs)
            xcb_ungrab_key(m_conn, g.keycode, g.root, g.mods);
    }

    static bool isVtSwitchKey(xcb_key_symbols_t *symbols, const xcb_keycode_t *codes)
    {
        for (const xcb_keycode_t *code = codes; *code != XCB_NO_SYMBOL; ++code) {
            for (int col = 0; col < 8; ++col) {
                const xcb_keysym_t sym = xcb_key_symbols_get_keysym(symbols, *code, col);
                if (sym >= kKeysymSwitchVt1 && sym <= kKeysymSwitchVt12)
                    return true;
            }
        }
        return false;
    }

    uint16_t numLockMask(xcb_key_symbols_t *symbols) const
    {
        uint16_t mask = 0;
        xcb_keycode_t *codes = xcb_key_symbols_get_keycode(symbols, kKeysymNumLock);
        if (!codes)
            return XCB_MOD_MASK_2;
        xcb_get_modifier_mapping_reply_t *reply =
            xcb_get_modifier_mapping_reply(m_conn, xcb_get_modifier_mapping(m_conn), nullptr);
        if (reply) {
            const xcb_keycode_t *map = xcb_get_modifier_mapping_keycodes(reply);
            const int perMod = reply->keycodes_per_modifier;
            for (int mod = 0; mod < 8 && !mask; ++mod) {
                for (int i = 0; i < perMod && !mask; ++i) {
                    for (const xcb_keycode_t *c = codes; *c != XCB_NO_SYMBOL; ++c) {
                        if (map[mod * perMod + i] == *c) {
                            mask = uint16_t(1u << mod);
                            break;
                        }
                    }
                }
            }
            std::free(reply);
        }
        std::free(codes);
        return mask ? mask : uint16_t(XCB_MOD_MASK_2);
    }

    std::function<void(const QString &)> m_fire;
    xcb_connection_t *m_conn = nullptr;
    QList<xcb_window_t> m_roots;
    QList<Grab> m_grabs;
    uint16_t m_numLock = XCB_MOD_MASK_2;
    xcb_keycode_t m_releasedKey = 0;
    xcb_timestamp_t m_releaseTime = 0;
    bool m_installed = false;
};

} // namespace
#endif // MM_X11_HOTKEYS

// ---------------------------------------------------------------------------------- Portal
#ifdef MIKMICK_HAVE_DBUS
namespace detail {

struct PortalShortcut
{
    QString id;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s)
{
    arg.beginStructure();
    arg << s.id << s.properties;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s)
{
    arg.beginStructure();
    arg >> s.id >> s.properties;
    arg.endStructure();
    return arg;
}

} // namespace detail
} // namespace mm::options

Q_DECLARE_METATYPE(mm::options::detail::PortalShortcut)

namespace mm::options {
namespace detail {

const QString kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kShortcutsIface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kRequestIface = QStringLiteral("org.freedesktop.portal.Request");

QVariantMap toMap(const QVariant &v)
{
    if (v.userType() == qMetaTypeId<QDBusArgument>())
        return qdbus_cast<QVariantMap>(v.value<QDBusArgument>());
    return v.toMap();
}

QString toPath(const QVariant &v)
{
    if (v.userType() == qMetaTypeId<QDBusObjectPath>())
        return v.value<QDBusObjectPath>().path();
    return v.toString();
}

class PortalShortcuts : public QObject
{
    Q_OBJECT
public:
    explicit PortalShortcuts(QObject *parent)
        : QObject(parent)
        , m_bus(QDBusConnection::sessionBus())
    {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<QList<PortalShortcut>>();
        m_bus.connect(kPortalService, kPortalPath, kShortcutsIface, QStringLiteral("Activated"), this,
                      SLOT(onActivated(QDBusMessage)));
    }

    ~PortalShortcuts() override { close(); }

    static bool available()
    {
        static int cached = -1;
        if (cached < 0) {
            QDBusMessage msg = QDBusMessage::createMethodCall(kPortalService, kPortalPath,
                                                              QStringLiteral("org.freedesktop.DBus.Properties"),
                                                              QStringLiteral("Get"));
            msg << kShortcutsIface << QStringLiteral("version");
            const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 3000);
            cached = reply.type() == QDBusMessage::ReplyMessage ? 1 : 0;
        }
        return cached == 1;
    }

    QString error() const { return m_error; }

    void bind(const HotkeyList &hotkeys)
    {
        close();
        m_pending = hotkeys;
        registerApp();
        const QString token = newToken();
        m_createPath = requestPath(token);
        watch(m_createPath);
        const QVariantMap options{{QStringLiteral("handle_token"), token},
                                  {QStringLiteral("session_handle_token"), newToken()}};
        call(kShortcutsIface, kPortalPath, QStringLiteral("CreateSession"), {options});
    }

    void close()
    {
        unwatch();
        m_createPath.clear();
        m_bindPath.clear();
        m_pending.clear();
        if (!m_session.isEmpty()) {
            m_bus.send(QDBusMessage::createMethodCall(kPortalService, m_session,
                                                      QStringLiteral("org.freedesktop.portal.Session"),
                                                      QStringLiteral("Close")));
            m_session.clear();
        }
    }

Q_SIGNALS:
    void activated(const QString &id);
    void errorChanged(const QString &error);

private Q_SLOTS:
    void onResponse(const QDBusMessage &message)
    {
        const QString path = message.path();
        if (path != m_watched)
            return;
        unwatch();
        const QList<QVariant> args = message.arguments();
        const uint code = args.value(0).toUInt();
        const QVariantMap results = toMap(args.value(1));
        if (path == m_createPath) {
            m_createPath.clear();
            if (code != 0) {
                setError(QStringLiteral("전역 단축키 세션을 만들지 못했습니다. ") + waylandHelp());
                return;
            }
            m_session = toPath(results.value(QStringLiteral("session_handle")));
            sendBind();
        } else if (path == m_bindPath) {
            m_bindPath.clear();
            if (code == 1)
                setError(QStringLiteral("전역 단축키 등록이 취소되었습니다."));
            else if (code != 0)
                setError(QStringLiteral("전역 단축키를 등록하지 못했습니다. ") + waylandHelp());
            else
                setError(QString());
        }
    }

    void onActivated(const QDBusMessage &message)
    {
        const QList<QVariant> args = message.arguments();
        if (m_session.isEmpty() || args.size() < 2 || toPath(args.at(0)) != m_session)
            return;
        Q_EMIT activated(args.at(1).toString());
    }

private:
    void sendBind()
    {
        QList<PortalShortcut> shortcuts;
        for (const auto &hk : std::as_const(m_pending)) {
            shortcuts << PortalShortcut{hk.first,
                                        {{QStringLiteral("description"), actionLabel(hk.first)},
                                         {QStringLiteral("preferred_trigger"), toPortalTrigger(hk.second)}}};
        }
        const QString token = newToken();
        m_bindPath = requestPath(token);
        watch(m_bindPath);
        call(kShortcutsIface, kPortalPath, QStringLiteral("BindShortcuts"),
             {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(shortcuts), QString(),
              QVariantMap{{QStringLiteral("handle_token"), token}}});
    }

    // Host (non-Flatpak) apps must announce their app id before any other portal call.
    void registerApp()
    {
        static bool done = false;
        if (done)
            return;
        done = true;
        QDBusMessage msg = QDBusMessage::createMethodCall(kPortalService, kPortalPath,
                                                          QStringLiteral("org.freedesktop.host.portal.Registry"),
                                                          QStringLiteral("Register"));
        msg << QStringLiteral("io.github.kaiht_ladiant.MikMick") << QVariantMap();
        m_bus.send(msg);
    }

    void call(const QString &iface, const QString &path, const QString &method, const QVariantList &args)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(kPortalService, path, iface, method);
        msg.setArguments(args);
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            if (!w->isError())
                return;
            unwatch();
            setError(QStringLiteral("전역 단축키 포털 오류: ") + w->error().message());
        });
    }

    QString requestPath(const QString &token) const
    {
        QString sender = m_bus.baseService();
        sender.remove(0, sender.startsWith(QLatin1Char(':')) ? 1 : 0);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        return kPortalPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
    }

    static QString newToken()
    {
        return QStringLiteral("mikmick%1").arg(QRandomGenerator::global()->generate());
    }

    // Subscribed before the call so a fast Response cannot be missed.
    void watch(const QString &path)
    {
        unwatch();
        m_watched = path;
        m_bus.connect(kPortalService, path, kRequestIface, QStringLiteral("Response"), this,
                      SLOT(onResponse(QDBusMessage)));
    }

    void unwatch()
    {
        if (m_watched.isEmpty())
            return;
        m_bus.disconnect(kPortalService, m_watched, kRequestIface, QStringLiteral("Response"), this,
                         SLOT(onResponse(QDBusMessage)));
        m_watched.clear();
    }

    void setError(const QString &error)
    {
        if (error == m_error)
            return;
        m_error = error;
        Q_EMIT errorChanged(error);
    }

    QDBusConnection m_bus;
    HotkeyList m_pending;
    QString m_session;
    QString m_createPath;
    QString m_bindPath;
    QString m_watched;
    QString m_error;
};

} // namespace detail
#endif // MIKMICK_HAVE_DBUS

// ---------------------------------------------------------------------------------- Manager
struct HotkeyManager::Private
{
    QString error;
#ifdef MM_X11_HOTKEYS
    std::unique_ptr<X11Grabber> x11;
#endif
#ifdef MIKMICK_HAVE_DBUS
    detail::PortalShortcuts *portal = nullptr;
#endif
};

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

HotkeyManager::~HotkeyManager()
{
    stop();
}

void HotkeyManager::apply(const QJsonObject &hotkeys)
{
    stop();
    HotkeyList list;
    for (auto it = hotkeys.begin(); it != hotkeys.end(); ++it) {
        const QString seq = buildHotkey(parseHotkey(it.value().toString()));
        if (!seq.isEmpty())
            list << qMakePair(it.key(), seq);
    }
    QString err;
    if (list.isEmpty()) {
        d->error = err;
        Q_EMIT errorChanged(err);
        return;
    }

    const bool wayland = sys::isWayland();
    const bool xcb = QGuiApplication::platformName() == QLatin1String("xcb");
    if (wayland) {
#ifdef MIKMICK_HAVE_DBUS
        if (detail::PortalShortcuts::available()) {
            if (!d->portal) {
                d->portal = new detail::PortalShortcuts(this);
                connect(d->portal, &detail::PortalShortcuts::activated, this, &HotkeyManager::triggered);
                connect(d->portal, &detail::PortalShortcuts::errorChanged, this, [this](const QString &e) {
                    d->error = e;
                    Q_EMIT errorChanged(e);
                });
            }
            d->portal->bind(list);
            d->error.clear();
            Q_EMIT errorChanged(d->error);
            return;
        }
#endif
        err = waylandHelp();
    }

    if (xcb) {
#ifdef MM_X11_HOTKEYS
        if (!d->x11) {
            d->x11 = std::make_unique<X11Grabber>([this](const QString &action) {
                // Deferred so a slot that opens a modal loop does not run inside the native event filter.
                QMetaObject::invokeMethod(this, [this, action]() { Q_EMIT triggered(action); },
                                          Qt::QueuedConnection);
            });
        }
        QStringList unknown;
        QStringList vtSwitch;
        const QStringList busy = d->x11->grab(list, &unknown, &vtSwitch);
        if (!wayland) {
            QStringList parts;
            if (!busy.isEmpty())
                parts << QStringLiteral("다른 프로그램이 이미 사용 중이어서 등록하지 못한 단축키: %1")
                             .arg(busy.join(QStringLiteral(", ")));
            if (!unknown.isEmpty())
                parts << QStringLiteral("지원하지 않는 키: %1").arg(unknown.join(QStringLiteral(", ")));
            if (!vtSwitch.isEmpty())
                parts << QStringLiteral("X 서버가 가상 콘솔 전환에 사용하는 단축키라서 동작하지 않습니다: %1")
                             .arg(vtSwitch.join(QStringLiteral(", ")));
            err = parts.join(QLatin1Char('\n'));
        }
#else
        if (!wayland)
            err = QStringLiteral("이 빌드는 X11 전역 단축키를 지원하지 않습니다 (XCB 없이 빌드됨).");
#endif
    } else if (!wayland) {
        err = QStringLiteral("현재 화면 환경에서는 전역 단축키를 사용할 수 없습니다.");
    }
    d->error = err;
    Q_EMIT errorChanged(err);
}

void HotkeyManager::stop()
{
#ifdef MM_X11_HOTKEYS
    if (d->x11)
        d->x11->ungrabAll();
#endif
#ifdef MIKMICK_HAVE_DBUS
    if (d->portal)
        d->portal->close();
#endif
    d->error.clear();
}

QString HotkeyManager::error() const
{
    return d->error;
}

bool HotkeyManager::supported() const
{
    if (sys::isWayland()) {
#ifdef MIKMICK_HAVE_DBUS
        return detail::PortalShortcuts::available();
#else
        return false;
#endif
    }
#ifdef MM_X11_HOTKEYS
    return X11Grabber::connection() != nullptr;
#else
    return false;
#endif
}

} // namespace mm::options

#ifdef MIKMICK_HAVE_DBUS
#include "HotkeyManager.moc"
#endif
