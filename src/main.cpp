// DecoLog — il log di stazione della famiglia Decodium.

#include "app/DecoLogController.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>

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
    parser.addOption(dbOption);
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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("decolog"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("DecoLog"), QStringLiteral("Main"));

    return app.exec();
}
