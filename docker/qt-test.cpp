#include <QApplication>
#include <QCursor>
#include <QPoint>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QUrl>
#include <QWindow>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

class OSCursor : public QObject
{
    Q_OBJECT

public:
    explicit OSCursor(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE QPoint getPosition() const
    {
        return QCursor::pos();
    }
};

static int env_int(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok ? value : fallback;
}

struct MotifWmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

enum {
    MWM_HINTS_FUNCTIONS = 1UL << 0,
    MWM_HINTS_DECORATIONS = 1UL << 1,
};

enum {
    MWM_FUNC_RESIZE = 1UL << 1,
    MWM_FUNC_MOVE = 1UL << 2,
    MWM_FUNC_MINIMIZE = 1UL << 3,
    MWM_FUNC_MAXIMIZE = 1UL << 4,
    MWM_FUNC_CLOSE = 1UL << 5,
};

static void apply_frameless_resize_hints(WId win_id)
{
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        return;
    }

    const Atom motif_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    MotifWmHints hints{};
    hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    hints.functions =
        MWM_FUNC_RESIZE | MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | MWM_FUNC_CLOSE;
    hints.decorations = 0;

    XChangeProperty(
        display,
        static_cast<Window>(win_id),
        motif_hints_atom,
        motif_hints_atom,
        32,
        PropModeReplace,
        reinterpret_cast<const unsigned char *>(&hints),
        5);
    XFlush(display);
    XCloseDisplay(display);
}

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    }
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArray("Basic"));
    }

    QCoreApplication::setApplicationName("window-test");
    QCoreApplication::setOrganizationName("test");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication app(argc, argv);
    app.setApplicationDisplayName("window-test");

    const QScreen *screen = app.primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    OSCursor cursor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("globalCursor", &cursor);
    engine.rootContext()->setContextProperty("screenAvailableWidth", available.width());
    engine.rootContext()->setContextProperty("screenAvailableHeight", available.height());
    engine.rootContext()->setContextProperty("testInitialWidth", env_int("TEST_WIDTH", 980));
    engine.rootContext()->setContextProperty("testInitialHeight", env_int("TEST_HEIGHT", 800));
    engine.rootContext()->setContextProperty("testMinWidth", env_int("TEST_MIN_WIDTH", 750));
    engine.rootContext()->setContextProperty("testMinHeight", env_int("TEST_MIN_HEIGHT", 450));
    const bool use_titlebar = env_int("TEST_SSD", 1) != 0;
    engine.rootContext()->setContextProperty("testUseTitleBar", use_titlebar);
    engine.rootContext()->setContextProperty("testTitleBarMinMax", env_int("TEST_TITLEBAR_MINMAX", 1) != 0);

    const QString qml_path = QCoreApplication::applicationDirPath() + "/qt-test.qml";
    engine.load(QUrl::fromLocalFile(qml_path));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (!use_titlebar && QGuiApplication::platformName() == QStringLiteral("xcb")) {
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
            apply_frameless_resize_hints(window->winId());
        }
    }

    return app.exec();
}

#include "qt-test.moc"