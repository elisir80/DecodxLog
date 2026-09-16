// decolog_udpsend — finge di essere Decodium sulla porta UDP.
//
//   decolog_udpsend                          un QSO FT2 di prova, come Decodium
//   decolog_udpsend --call K1AB --mode FT8   nominativo e modo a scelta
//   decolog_udpsend --only-qsologged         solo QSOLogged, come certi client
//   decolog_udpsend --status                 solo un Status (frequenza, DX call)
#include "core/Adif.h"
#include "core/Bands.h"
#include "core/WsjtxProtocol.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QTextStream>
#include <QThread>
#include <QUdpSocket>

using namespace decolog::core;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser p;
    p.addHelpOption();
    QCommandLineOption host("host", "Destination address.", "addr", "127.0.0.1");
    QCommandLineOption port("port", "Destination port.", "port", "2237");
    QCommandLineOption call("call", "DX callsign.", "call", "DL1AB");
    QCommandLineOption mode("mode", "Mode as Decodium shows it (FT2, FT8, FT4...).", "mode", "FT2");
    QCommandLineOption freq("freq", "Dial frequency in Hz.", "hz", "14084000");
    QCommandLineOption myCall("mycall", "Station callsign.", "call", "IU8LMC");
    QCommandLineOption client("client", "Client id.", "id", "Decodium");
    QCommandLineOption onlyQsoLogged("only-qsologged", "Send only QSOLogged, no LoggedADIF.");
    QCommandLineOption statusOnly("status", "Send a Status message only.");
    p.addOptions({host, port, call, mode, freq, myCall, client, onlyQsoLogged, statusOnly});
    p.process(app);

    const QHostAddress to(p.value(host));
    const quint16 toPort = p.value(port).toUShort();
    const QString id = p.value(client);
    QUdpSocket socket;
    QTextStream out(stdout);

    auto send = [&](const QByteArray& datagram, const char* what) {
        const qint64 n = socket.writeDatagram(datagram, to, toPort);
        out << what << ": " << n << " bytes -> " << to.toString() << ':' << toPort << Qt::endl;
    };

    send(wsjtx::buildHeartbeat(id, {3, "1.0.637", "udpsend"}), "Heartbeat");

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const quint64 hz = p.value(freq).toULongLong();

    wsjtx::Status st;
    st.dialFrequencyHz = hz;
    st.mode = p.value(mode);
    st.dxCall = p.value(call);
    st.deCall = p.value(myCall);
    st.deGrid = "JN71DC";
    send(wsjtx::buildStatus(id, st), "Status");
    if (p.isSet(statusOnly))
        return 0;

    wsjtx::QsoLogged q;
    q.timeOn = now.addSecs(-45);
    q.timeOff = now;
    q.dxCall = p.value(call);
    q.dxGrid = "JO62";
    q.txFrequencyHz = hz;
    q.mode = p.value(mode);
    q.reportSent = "-10";
    q.reportReceived = "-05";
    q.myCall = p.value(myCall);
    q.myGrid = "JN71DC";
    send(wsjtx::buildQsoLogged(id, q), "QSOLogged");

    if (!p.isSet(onlyQsoLogged)) {
        QThread::msleep(20);
        AdifRecord r;
        r.set("CALL", q.dxCall);
        r.set("GRIDSQUARE", q.dxGrid);
        r.set("MODE", q.mode);
        adif::normalizeMode(r);
        r.set("RST_SENT", q.reportSent);
        r.set("RST_RCVD", q.reportReceived);
        r.set("QSO_DATE", q.timeOn.toString("yyyyMMdd"));
        r.set("TIME_ON", q.timeOn.toString("HHmmss"));
        r.set("QSO_DATE_OFF", q.timeOff.toString("yyyyMMdd"));
        r.set("TIME_OFF", q.timeOff.toString("HHmmss"));
        const double mhz = static_cast<double>(hz) / 1e6;
        r.set("BAND", bands::fromMhz(mhz));
        r.set("FREQ", QString::number(mhz, 'f', 6));
        r.set("STATION_CALLSIGN", q.myCall);
        r.set("MY_GRIDSQUARE", q.myGrid);
        const QString programId = id + " FT2 1.0.637";
        QByteArray adif = QString("\n<adif_ver:5>3.1.0\n<programid:%1>%2\n<EOH>\n")
                              .arg(programId.size()).arg(programId).toUtf8();
        adif += adif::writeRecord(r).toUtf8();
        send(wsjtx::buildLoggedAdif(id, adif), "LoggedADIF");
    }
    return 0;
}
