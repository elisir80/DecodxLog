// decodxlog_udpsend — finge di essere Decodium sulla porta UDP.
//
//   decodxlog_udpsend                          un QSO FT2 di prova, come Decodium
//   decodxlog_udpsend --call K1AB --mode FT8   nominativo e modo a scelta
//   decodxlog_udpsend --only-qsologged         solo QSOLogged, come certi client
//   decodxlog_udpsend --status                 solo un Status (frequenza, DX call)
#include "core/Adif.h"
#include "core/Bands.h"
#include "core/WsjtxProtocol.h"

#include <QCommandLineParser>
#include <QElapsedTimer>
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
    QCommandLineOption stream("stream", "Behave like a running Decodium for N seconds: Status several times a second, "
                                        "a burst of decodes every 15 s, the DX call changing.", "secs");
    QCommandLineOption statusRate("status-rate", "Status messages per second with --stream.", "n", "5");
    p.addOptions({host, port, call, mode, freq, myCall, client, onlyQsoLogged, statusOnly, stream, statusRate});
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

    if (p.isSet(stream)) {
        const int secs = p.value(stream).toInt();
        const int rate = qMax(1, p.value(statusRate).toInt());
        const QStringList dx{"JA1ABC", "K1XYZ", "PY2AA", "VK3BB", "DL1CC", "ZS6DD", "UA9EE", "W1AW", "LU1FF", "OH2GG"};
        QElapsedTimer clock;
        clock.start();
        int tick = 0;
        int lastCycle = -1;
        while (clock.elapsed() < secs * 1000) {
            wsjtx::Status st;
            st.dialFrequencyHz = hz;
            st.mode = p.value(mode);
            st.dxCall = dx.at((tick / (rate * 7)) % dx.size());
            st.deCall = p.value(myCall);
            st.deGrid = "JN71DC";
            st.transmitting = (tick / (rate * 15)) % 2 == 1;
            socket.writeDatagram(wsjtx::buildStatus(id, st), to, toPort);
            const int cycle = static_cast<int>(clock.elapsed() / 15000);
            if (cycle != lastCycle) {
                lastCycle = cycle;
                // Una tornata di decodifiche, come alla fine di un periodo FT8.
                for (int i = 0; i < 40; ++i) {
                    wsjtx::Decode d;
                    d.time = QTime::currentTime();
                    d.snr = -20 + i % 25;
                    d.deltaFrequency = 300 + i * 60;
                    d.mode = "~";
                    d.message = QString("CQ %1%2 JN%3").arg(dx.at(i % dx.size())).arg(i).arg(10 + i % 80);
                    socket.writeDatagram(wsjtx::buildDecode(id, d), to, toPort);
                }
                socket.writeDatagram(wsjtx::buildHeartbeat(id, {3, "1.0.637", "udpsend"}), to, toPort);
            }
            ++tick;
            QThread::msleep(1000 / rate);
        }
        out << "stream: " << tick << " Status sent" << Qt::endl;
        return 0;
    }

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
