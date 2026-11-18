// yeptes — a plain text notepad with Markdown, JSON, and HTML views.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "jsonmodel.h"
#include "offline.h"
#include "portalfilepicker.h"
#include "tablemodel.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    // The commands for scripts and agents run without a display.
    if (Cli::handles(arguments)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("yeptes");
        app.setApplicationVersion("0.3.0");
        return Cli::run(arguments);
    }

    QGuiApplication app(argc, argv);
    app.setOrganizationName("yeptes");
    app.setApplicationName("yeptes");
    app.setApplicationVersion("0.3.0");

    // Matches the window to yeptes.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("yeptes");
    app.setWindowIcon(QIcon::fromTheme("yeptes"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    JsonModel jsonModel;
    TableModel tableModel;

    backend.setAccent(theme.accent());
    backend.setPalette(theme.colors());
    QObject::connect(&theme, &Theme::changed, &backend, [&] {
        backend.setAccent(theme.accent());
        backend.setPalette(theme.colors());
    });

    // "yeptes notes.md" and "yeptes open notes.md" both open the file.
    if (!arguments.isEmpty() && arguments.first() == "open")
        arguments.removeFirst();
    if (!arguments.isEmpty())
        backend.load(arguments.first());

    QQmlApplicationEngine engine;
    OfflineFactory offline;
    engine.setNetworkAccessManagerFactory(&offline);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("jsonModel", &jsonModel);
    engine.rootContext()->setContextProperty("tableModel", &tableModel);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
