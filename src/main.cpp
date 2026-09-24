// DecoDXLog — il log di stazione della famiglia Decodium.

#include "CrashLog.h"
#include "app/DecoLogController.h"
#include "ThemeManager.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

// Copia una cartella intera, senza toccare quello che c'e' gia' di la'.
void copyTree(const QString& from, const QString& to)
{
    QDir source(from);
    if (!source.exists())
        return;
    QDir().mkpath(to);
    for (const QFileInfo& entry : source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isDir())
            copyTree(entry.absoluteFilePath(), target);
        else if (!QFile::exists(target))
            QFile::copy(entry.absoluteFilePath(), target);
    }
}

// Fino alla 0.7.0 il programma si chiamava DecoLog, e il log, le impostazioni e
// le sue cartelle portavano quel nome. Al primo avvio col nome nuovo ci si
// porta dietro tutto — *copiando*: quello di prima resta dov'e', cosi' se
// qualcosa va storto il log di vent'anni e' ancora al suo posto.
void bringForwardTheOldName()
{
    // Per le prove: DECODXLOG_DATA_ROOT sposta tutto in una cartella qualsiasi,
    // cosi' il trasloco si puo' provare senza mettere le mani nel log vero.
    const QString testRoot = qEnvironmentVariable("DECODXLOG_DATA_ROOT");
    const QString roaming = testRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QDir(testRoot).filePath(QStringLiteral("Decodium/DecoDXLog"));
    const QString localData = testRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        : QDir(testRoot).filePath(QStringLiteral("Decodium-local/DecoDXLog"));
    for (const QString& now : {roaming, localData}) {
        if (now.isEmpty() || QDir(now).exists())
            continue;
        const QString before = QDir(now).absolutePath().left(
                                   QDir(now).absolutePath().lastIndexOf(QLatin1Char('/')) + 1)
                               + QStringLiteral("DecoLog");
        if (QDir(before).exists())
            copyTree(before, now);
    }
    // Il log: si chiamava decolog.sqlite. Arriva qui con la cartella, e da
    // quella copia si fa quella col nome nuovo; poi la copia col nome vecchio
    // se ne va — l'originale, quello vero, resta dov'era.
    const QString db = QDir(roaming).filePath(QStringLiteral("decodxlog.sqlite"));
    const QString oldDb = QDir(roaming).filePath(QStringLiteral("decolog.sqlite"));
    if (QFile::exists(oldDb)) {
        if (!QFile::exists(db))
            QFile::copy(oldDb, db);
        if (QFile::exists(db))
            QFile::remove(oldDb);
    }

    // Le impostazioni: un .ini accanto alla cartella, col nome del programma.
    QSettings settings;
    const QString ini = testRoot.isEmpty()
        ? settings.fileName()
        : QDir(testRoot).filePath(QStringLiteral("Decodium/DecoDXLog.ini"));
    if (!ini.isEmpty() && !QFile::exists(ini)) {
        QString oldIni = ini;
        oldIni.replace(QStringLiteral("DecoDXLog.ini"), QStringLiteral("DecoLog.ini"));
        if (oldIni != ini && QFile::exists(oldIni)) {
            QDir().mkpath(QFileInfo(ini).absolutePath());
            QFile::copy(oldIni, ini);
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    // I menu li disegna DecoDXLog, non Windows. Da Qt 6.8 i menu di QML possono
    // diventare menu nativi del sistema: quelli non sanno niente del tema e su
    // uno sfondo scuro scrivono nero su nero — sottomenu, tendine e il menu del
    // tasto destro dentro i campi di testo diventavano illeggibili.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuWindows);
    QGuiApplication app(argc, argv);
#if defined(Q_OS_MACOS)
    const QStringList uiFontCandidates = {QStringLiteral("SF Pro Text"), QStringLiteral("Helvetica Neue"), QStringLiteral("Arial")};
#elif defined(Q_OS_WIN)
    const QStringList uiFontCandidates = {QStringLiteral("Segoe UI"), QStringLiteral("Arial")};
#else
    const QStringList uiFontCandidates = {QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans"), QStringLiteral("Liberation Sans")};
#endif
    const QStringList installedFonts = QFontDatabase::families();
    for (const QString& family : uiFontCandidates) {
        if (installedFonts.contains(family)) {
            app.setFont(QFont(family));
            break;
        }
    }
    app.setApplicationName(QStringLiteral("DecoDXLog"));
    app.setOrganizationName(QStringLiteral("Decodium"));
    app.setApplicationVersion(QStringLiteral(DECODXLOG_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/decolog/decodxlog.png")));

    // Come Decodium: impostazioni in un .ini leggibile, non nel registro. Va
    // detto subito: la lingua si legge due righe piu' sotto, e prima di questa
    // riga QSettings guardava nel registro — dove la lingua scelta non c'e'
    // mai, cosi' chi sceglieva l'italiano su un Windows inglese si ritrovava
    // l'inglese lo stesso.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // Per le prove: --settings <cartella> tiene le impostazioni li' dentro
    // invece che fra quelle vere. Si legge a mano dagli argomenti perche' la
    // lingua si sceglie prima che il parser esista.
    const QStringList rawArguments = app.arguments();
    const qsizetype settingsAt = rawArguments.indexOf(QStringLiteral("--settings"));
    if (settingsAt >= 0 && settingsAt + 1 < rawArguments.size()) {
        const QString where = rawArguments.at(settingsAt + 1);
        QDir().mkpath(where);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, where);
    }

    // Prima di leggere qualsiasi cosa: se qui c'e' ancora la roba di quando il
    // programma si chiamava DecoLog, ce la si porta dietro.
    bringForwardTheOldName();

    // Se il programma cade, un file con la strada che l'ha portato li': il
    // registro di Windows tiene solo l'ultimo passo, e non basta per trovarlo.
    // Nelle prove sta nella cartella delle impostazioni di prova.
    decolog::crashlog::install(
        settingsAt >= 0 && settingsAt + 1 < rawArguments.size()
            ? rawArguments.at(settingsAt + 1)
            : QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("crash")),
        QStringLiteral(DECODXLOG_VERSION));

    // Lingua dell'interfaccia: quella scelta, o quella del sistema. L'inglese e'
    // la lingua dei sorgenti, quindi non ha un file da caricare.
    const QString configured = QSettings().value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    // "auto" vuol dire come il sistema. Il cinese va distinto: tradizionale a
    // Taiwan, Hong Kong e Macao, semplificato altrove — e la differenza non si
    // vede dalle prime due lettere.
    QString language = configured;
    if (configured == QLatin1String("auto")) {
        const QString whole = QLocale::system().name();          // it_IT, zh_TW…
        language = whole.left(2);
        if (language == QLatin1String("zh")) {
            language = whole.endsWith(QLatin1String("TW")) || whole.endsWith(QLatin1String("HK"))
                       || whole.endsWith(QLatin1String("MO"))
                       ? QStringLiteral("zh_TW") : QStringLiteral("zh");
        }
    }
    QTranslator appTranslator;
    QTranslator qtTranslator;
    if (language != QLatin1String("en")) {
        // Se la lingua chiesta non c'e', si prova quella senza variante
        // (zh_TW → zh); altrimenti si resta in inglese, mai a meta'.
        if (appTranslator.load(QStringLiteral(":/i18n/decodxlog_") + language)
            || (language.contains(QLatin1Char('_'))
                && appTranslator.load(QStringLiteral(":/i18n/decodxlog_")
                                      + language.section(QLatin1Char('_'), 0, 0))))
            app.installTranslator(&appTranslator);
        // Le finestre di dialogo di Qt (se presenti accanto all'eseguibile).
        if (qtTranslator.load(QStringLiteral("qtbase_") + language.section(QLatin1Char('_'), 0, 0),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&qtTranslator);
    }
    // I caratteri vengono dopo la lingua: Consolas e Segoe UI non hanno gli
    // ideogrammi, e in giapponese o in cinese mezza finestra sarebbe una fila
    // di quadratini. In quelle lingue si parte da MS Gothic, NSimSun, MingLiU.
    decodium::ui::ThemeManager::setLanguage(language);

    // L'interfaccia disegna le proprie superfici; uno stile di piattaforma
    // combatterebbe i pannelli invece di aiutarli.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DecoDXLog station logbook"));
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
    // Gia' letta prima del parser: qui sta solo perche' il parser non la
    // prenda per un errore.
    QCommandLineOption settingsOption(QStringLiteral("settings"),
                                      QStringLiteral("Keep settings in this folder (for tests)."),
                                      QStringLiteral("folder"));
    parser.addOption(settingsOption);
    // Idem per la propagazione: il XML del Sole letto da un file.
    QCommandLineOption solarOption(QStringLiteral("solar"), QStringLiteral("Read the solar XML from a file."),
                                   QStringLiteral("file"));
    parser.addOption(solarOption);
    // Rotore per una prova: "host:porta", o "rotctld@host:porta".
    QCommandLineOption rotorOption(QStringLiteral("rotor"), QStringLiteral("Use this rotor gateway."),
                                   QStringLiteral("[backend@]host:port"));
    parser.addOption(rotorOption);
    // Radio per una prova: "host:porta" di un rigctld gia' in piedi.
    QCommandLineOption rigOption(QStringLiteral("rig"), QStringLiteral("Use this rigctld (Hamlib)."),
                                 QStringLiteral("host:port"));
    parser.addOption(rigOption);
    // Server del Cloud per una prova, senza toccare le impostazioni.
    QCommandLineOption cloudOption(QStringLiteral("cloud"), QStringLiteral("Use this DecoDXLog Cloud server."),
                                   QStringLiteral("url"));
    parser.addOption(cloudOption);
    // Dove chiedere se c'e' una versione nuova: per le prove, senza andare
    // davvero su GitHub.
    QCommandLineOption updatesOption(QStringLiteral("updates-url"),
                                     QStringLiteral("Ask this address for the latest release."),
                                     QStringLiteral("url"));
    parser.addOption(updatesOption);
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
        dbPath = QDir(dir).filePath(QStringLiteral("decodxlog.sqlite"));
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
    if (parser.isSet(rotorOption)) {
        QString value = parser.value(rotorOption);
        QString backend = QStringLiteral("decorotor");
        if (value.contains(QLatin1Char('@'))) {
            backend = value.section(QLatin1Char('@'), 0, 0);
            value = value.section(QLatin1Char('@'), 1);
        }
        if (auto* rotor = qobject_cast<decolog::app::RotorController*>(controller.rotor())) {
            rotor->overrideConnection(backend, value.section(QLatin1Char(':'), 0, 0),
                                      value.section(QLatin1Char(':'), 1).toInt());
        }
    } else {
        controller.startRotor();
    controller.startCloud(!parser.isSet(grabOption));
    }
    if (parser.isSet(updatesOption)) {
        if (auto* upd = qobject_cast<decolog::app::UpdateController*>(controller.updates()))
            upd->overrideUrl(QUrl(parser.value(updatesOption)));
    }
    if (parser.isSet(rigOption)) {
        const QString value = parser.value(rigOption);
        if (auto* rig = qobject_cast<decolog::app::RigController*>(controller.rig())) {
            rig->overrideConnection(value.section(QLatin1Char(':'), 0, 0),
                                    value.section(QLatin1Char(':'), 1).toInt());
        }
    }
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
    if (parser.isSet(solarOption)) {
        QFile solarFile(parser.value(solarOption));
        if (solarFile.open(QIODevice::ReadOnly)) {
            if (auto* solar = qobject_cast<decolog::app::SolarController*>(controller.solar()))
                solar->injectXml(solarFile.readAll());
        }
    }
    if (parser.isSet(cloudOption)) {
        if (auto* cloud = qobject_cast<decolog::app::CloudController*>(controller.cloud()))
            cloud->overrideServer(parser.value(cloudOption));
    }
    if (parser.isSet(importOption))
        controller.importAdif(QUrl::fromLocalFile(parser.value(importOption)));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("decolog"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.rootContext()->setContextProperty(QStringLiteral("startupShow"), parser.value(showOption));
    engine.loadFromModule(QStringLiteral("DecoDXLog"), QStringLiteral("Main"));
    // La spia dei blocchi si accende a interfaccia in piedi: i secondi
    // dell'avvio sono avvio, non un blocco, e segnalarli sarebbe gridare al
    // lupo alla prima riga del registro.
    QTimer::singleShot(3000, &controller, [&controller] { controller.startFreezeWatch(); });
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

    decolog::crashlog::setStage("running");
    // Per provare il biglietto: DECODXLOG_CRASH_TEST=1 fa cadere il programma
    // di proposito, un attimo dopo l'avvio.
    if (qEnvironmentVariableIntValue("DECODXLOG_CRASH_TEST") == 1) {
        QTimer::singleShot(1500, &app, [] {
            volatile int* nowhere = nullptr;
            *nowhere = 1;
        });
    }
    const int code = app.exec();
    decolog::crashlog::setStage("quitting");
    return code;
}
