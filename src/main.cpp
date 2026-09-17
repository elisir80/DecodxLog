// DecoLog — il log di stazione della famiglia Decodium.

#include "app/DecoLogController.h"
#include "ThemeManager.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DecoLog"));
    app.setOrganizationName(QStringLiteral("Decodium"));
    app.setApplicationVersion(QStringLiteral(DECOLOG_VERSION));
    // Come Decodium: impostazioni in un .ini leggibile, non nel registro.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // L'interfaccia disegna le proprie superfici; uno stile di piattaforma
    // combatterebbe i pannelli invece di aiutarli.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DecoLog station logbook"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dbOption(QStringLiteral("db"), QStringLiteral("Log database file."), QStringLiteral("path"));
    QCommandLineOption portOption(QStringLiteral("port"), QStringLiteral("UDP port (overrides settings)."),
                                  QStringLiteral("port"));
    QCommandLineOption importOption(QStringLiteral("import"), QStringLiteral("Import an ADIF file at startup."),
                                    QStringLiteral("file"));
    // Per provare l'interfaccia e fare le schermate senza cliccare: tema e finestra
    // di dialogo aperta all'avvio.
    QCommandLineOption themeOption(QStringLiteral("theme"), QStringLiteral("Theme to use (saved)."), QStringLiteral("name"));
    QCommandLineOption showOption(QStringLiteral("show"),
                                  QStringLiteral("Open at startup: new, qso:<id>, profiles, setup:<page>."),
                                  QStringLiteral("what"));
    QCommandLineOption grabOption(QStringLiteral("grab"),
                                  QStringLiteral("Save a screenshot of the window to this PNG after startup, then quit."),
                                  QStringLiteral("file"));
    parser.addOption(grabOption);
    parser.addOption(themeOption);
    parser.addOption(showOption);
    parser.addOption(dbOption);
    parser.addOption(importOption);
    parser.addOption(portOption);
    parser.process(app);

    QString dbPath = parser.value(dbOption);
    if (dbPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        dbPath = QDir(dir).filePath(QStringLiteral("decolog.sqlite"));
    }

    decolog::app::DecoLogController controller;
    controller.openDatabase(dbPath);
    if (parser.isSet(portOption))
        controller.overrideUdpPort(parser.value(portOption).toInt());
    controller.startListening();
    controller.startDecoLink();
    if (parser.isSet(importOption))
        controller.importAdif(QUrl::fromLocalFile(parser.value(importOption)));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("decolog"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.rootContext()->setContextProperty(QStringLiteral("startupShow"), parser.value(showOption));
    engine.loadFromModule(QStringLiteral("DecoLog"), QStringLiteral("Main"));
    if (parser.isSet(themeOption)) {
        if (auto* theme = engine.singletonInstance<decodium::ui::ThemeManager*>(QStringLiteral("Decodium.UI"),
                                                                                  QStringLiteral("Theme")))
            theme->setCurrentTheme(parser.value(themeOption));
    }

    // Schermata senza toccare il desktop: con QT_QPA_PLATFORM=offscreen la finestra
    // non compare nemmeno, e nessun clic finisce su un altro programma.
    if (parser.isSet(grabOption)) {
        const QString file = parser.value(grabOption);
        QTimer::singleShot(3000, &app, [&engine, file] {
            if (!engine.rootObjects().isEmpty()) {
                if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()))
                    window->grabWindow().save(file);
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
