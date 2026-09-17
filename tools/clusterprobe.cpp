// decolog_clusterprobe — si collega a una fonte di spot e stampa quello che arriva:
// per provare un nodo, RBN o POTA senza aprire DecoLog.
//
//   decolog_clusterprobe --host dx.iz7auh.net --port 8000 --call IU8LMC --seconds 60
//   decolog_clusterprobe --type rbn --host telnet.reversebeacon.net --port 7001 --call IU8LMC
//   decolog_clusterprobe --type pota --seconds 70
#include "core/ClusterConnection.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace decolog::core;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("decolog_clusterprobe"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser p;
    p.addHelpOption();
    QCommandLineOption type("type", "cluster | rbn | pota", "type", "cluster");
    QCommandLineOption host("host", "Host.", "host", "dx.iz7auh.net");
    QCommandLineOption port("port", "Port.", "port", "8000");
    QCommandLineOption call("call", "Login callsign.", "call");
    QCommandLineOption commands("commands", "Commands after login, separated by ';'.", "commands");
    QCommandLineOption seconds("seconds", "How long to listen.", "seconds", "45");
    p.addOptions({type, host, port, call, commands, seconds});
    p.process(app);

    ClusterSource s;
    s.id = QStringLiteral("probe");
    s.name = p.value(host);
    s.type = p.value(type);
    s.host = p.value(host);
    s.port = p.value(port).toInt();
    s.login = p.value(call);
    s.commands = p.value(commands).replace(QLatin1Char(';'), QLatin1Char('\n'));

    QTextStream out(stdout);
    ClusterConnection c(s);
    QObject::connect(&c, &ClusterConnection::stateChanged, [&] {
        out << "[state] " << c.stateText() << Qt::endl;
    });
    QObject::connect(&c, &ClusterConnection::lineReceived, [&](const QString& line) {
        out << "[line]  " << line << Qt::endl;
    });
    QObject::connect(&c, &ClusterConnection::spotReceived, [&](const Spot& spot) {
        out << "[spot]  " << spot.time.toString(QStringLiteral("HH:mm")) << ' ' << spot.freqKhz << ' ' << spot.dxCall
            << ' ' << spot.band << ' ' << spot.mode << " de " << spot.spotter
            << (spot.hasSnr ? QStringLiteral(" %1 dB").arg(spot.snr) : QString())
            << (spot.potaRef.isEmpty() ? QString() : QStringLiteral(" POTA ") + spot.potaRef)
            << " | " << spot.comment << Qt::endl;
    });
    c.start();
    QTimer::singleShot(p.value(seconds).toInt() * 1000, &app, [&] {
        out << "[done]  " << c.spotCount() << " spots" << Qt::endl;
        c.stop();
        app.quit();
    });
    return app.exec();
}
