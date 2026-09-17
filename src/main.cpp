// DecoLog — il log di stazione della famiglia Decodium.

#include "app/DecoLogController.h"
#include "ThemeManager.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    // Per le prove: spot presi da un file (una riga "DX de" o JSON HamAlert ciascuna)
    // invece che dalla rete.
    QCommandLineOption spotsOption(QStringLiteral("spots"), QStringLiteral("Feed cluster spots from a file."),
                                   QStringLiteral("file"));
    parser.addOption(spotsOption);
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
    // Una schermata di prova non si collega ai nodi veri.
    if (!parser.isSet(grabOption) && !parser.isSet(spotsOption))
        controller.startCluster();
    if (parser.isSet(spotsOption)) {
        QFile spotFile(parser.value(spotsOption));
        if (spotFile.open(QIODevice::ReadOnly)) {
            auto* cluster = qobject_cast<decolog::app::ClusterController*>(controller.cluster());
            // Una prova non parla dagli altoparlanti.
            cluster->setMuted(parser.isSet(grabOption));
            for (const QByteArray& line : spotFile.readAll().split('\n'))
                cluster->injectLine(QString::fromUtf8(line).trimmed());
        }
    }
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
            // Le altre finestre aperte (cluster, logbook separato) accanto: file-2.png...
            int n = 2;
            for (QWindow* w : QGuiApplication::topLevelWindows()) {
                auto* quick = qobject_cast<QQuickWindow*>(w);
                if (!quick || !quick->isVisible() || (!engine.rootObjects().isEmpty() && quick == engine.rootObjects().constFirst()))
                    continue;
                const QFileInfo info(file);
                quick->grabWindow().save(info.dir().filePath(QStringLiteral("%1-%2.%3").arg(info.completeBaseName()).arg(n++).arg(info.suffix())));
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
