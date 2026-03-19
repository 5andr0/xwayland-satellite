#include <QApplication>
#include <QCursor>
#include <QPoint>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QUrl>

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

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArray("Basic"));

    QCoreApplication::setApplicationName("window-test");
    QCoreApplication::setOrganizationName("test");

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
    engine.rootContext()->setContextProperty("testUseTitleBar", env_int("TEST_USE_TITLEBAR", 0) != 0);
    engine.rootContext()->setContextProperty("testTitleBarMinMax", env_int("TEST_TITLEBAR_MINMAX", 0) != 0);

    const QString qml_path = QCoreApplication::applicationDirPath() + "/qt-test.qml";
    engine.load(QUrl::fromLocalFile(qml_path));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}

#include "qt-test.moc"