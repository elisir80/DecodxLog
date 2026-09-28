#include "core/FlrigControl.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QXmlStreamReader>

namespace decolog::core {

namespace flrig {

namespace {

QString escape(const QString& s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QLatin1String("&amp;"))
        .replace(QLatin1Char('<'), QLatin1String("&lt;"))
        .replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return out;
}

QString valueXml(const QVariant& v)
{
    switch (v.metaType().id()) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Bool:
        return QStringLiteral("<value><i4>%1</i4></value>").arg(v.toLongLong());
    case QMetaType::Double:
        return QStringLiteral("<value><double>%1</double></value>").arg(QString::number(v.toDouble(), 'f', 1));
    default:
        return QStringLiteral("<value><string>%1</string></value>").arg(escape(v.toString()));
    }
}

// Legge un <value> gia' aperto fino alla sua chiusura.
QVariant readValue(QXmlStreamReader& xml)
{
    QVariant out;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement() && xml.name() == QLatin1String("value"))
            return out;
        if (xml.isCharacters() && !xml.isWhitespace() && !out.isValid())
            out = xml.text().toString();     // <value>testo</value> senza tipo = stringa
        if (!xml.isStartElement())
            continue;
        const auto n = xml.name();
        if (n == QLatin1String("string")) {
            out = xml.readElementText();
        } else if (n == QLatin1String("i4") || n == QLatin1String("int")) {
            out = xml.readElementText().trimmed().toLongLong();
        } else if (n == QLatin1String("double")) {
            out = xml.readElementText().trimmed().toDouble();
        } else if (n == QLatin1String("boolean")) {
            out = xml.readElementText().trimmed() == QLatin1String("1");
        } else if (n == QLatin1String("array")) {
            QVariantList list;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("array"))
                    break;
                if (xml.isStartElement() && xml.name() == QLatin1String("value"))
                    list << readValue(xml);
            }
            out = list;
        } else if (n == QLatin1String("struct")) {
            QVariantMap map;
            QString key;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("struct"))
                    break;
                if (xml.isStartElement() && xml.name() == QLatin1String("name"))
                    key = xml.readElementText();
                else if (xml.isStartElement() && xml.name() == QLatin1String("value"))
                    map.insert(key, readValue(xml));
            }
            out = map;
        }
    }
    return out;
}

} // namespace

QByteArray request(const QString& method, const QVariantList& params)
{
    QString body = QStringLiteral("<?xml version=\"1.0\"?><methodCall><methodName>%1</methodName><params>").arg(escape(method));
    for (const QVariant& p : params)
        body += QStringLiteral("<param>%1</param>").arg(valueXml(p));
    body += QStringLiteral("</params></methodCall>");
    return body.toUtf8();
}

QVariant parseResponse(const QByteArray& body, QString* fault)
{
    QXmlStreamReader xml(body);
    bool inFault = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("fault"))
            inFault = true;
        if (xml.isStartElement() && xml.name() == QLatin1String("value")) {
            const QVariant v = readValue(xml);
            if (inFault) {
                if (fault) {
                    const QVariantMap m = v.toMap();
                    *fault = m.value(QStringLiteral("faultString"), QStringLiteral("fault")).toString();
                }
                return {};
            }
            return v;
        }
    }
    if (fault && xml.hasError())
        *fault = xml.errorString();
    return {};
}

QString toHamlibMode(const QString& flrigMode)
{
    const QString m = flrigMode.trimmed().toUpper();
    if (m.startsWith(QLatin1String("CW")))
        return m.contains(QLatin1Char('R')) && m != QLatin1String("CW") ? QStringLiteral("CWR") : QStringLiteral("CW");
    if (m == QLatin1String("DIGU") || m == QLatin1String("DATA-U") || m == QLatin1String("PKT-U")
        || m == QLatin1String("USB-D") || m == QLatin1String("D-USB") || m == QLatin1String("DATA-USB")
        || m == QLatin1String("USB-D1") || m == QLatin1String("PKTUSB") || m == QLatin1String("DATA"))
        return QStringLiteral("PKTUSB");
    if (m == QLatin1String("DIGL") || m == QLatin1String("DATA-L") || m == QLatin1String("PKT-L")
        || m == QLatin1String("LSB-D") || m == QLatin1String("D-LSB") || m == QLatin1String("DATA-LSB")
        || m == QLatin1String("PKTLSB"))
        return QStringLiteral("PKTLSB");
    if (m.startsWith(QLatin1String("RTTY")) || m.startsWith(QLatin1String("FSK")))
        return m.contains(QLatin1Char('R')) && m.size() > 4 ? QStringLiteral("RTTYR") : QStringLiteral("RTTY");
    if (m.startsWith(QLatin1String("USB")))
        return QStringLiteral("USB");
    if (m.startsWith(QLatin1String("LSB")))
        return QStringLiteral("LSB");
    if (m.startsWith(QLatin1String("AM")))
        return QStringLiteral("AM");
    if (m.startsWith(QLatin1String("FM")) || m.startsWith(QLatin1String("NFM")))
        return QStringLiteral("FM");
    return m;
}

QString fromHamlibMode(const QString& hamlibMode, const QStringList& available)
{
    const QString want = hamlibMode.trimmed().toUpper();
    // Il primo dei modi della radio che torna uguale: cosi' "PKTUSB" diventa
    // DATA-U su una Yaesu e USB-D su una Icom, senza tabelle per ogni modello.
    for (const QString& m : available) {
        if (toHamlibMode(m) == want)
            return m;
    }
    return want;
}

} // namespace flrig

FlrigControl::FlrigControl(QObject* parent)
    : RigLink(parent)
    , m_net(new QNetworkAccessManager(this))
{
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &FlrigControl::refresh);
}

void FlrigControl::connectTo(const QString& address)
{
    disconnectFromRig();
    QString a = address.trimmed().isEmpty() ? QStringLiteral("127.0.0.1:12345") : address.trimmed();
    if (!a.contains(QLatin1Char(':')))
        a += QStringLiteral(":12345");
    m_url = QUrl(QStringLiteral("http://%1/RPC2").arg(a));
    m_active = true;
    m_failures = 0;
    m_status = QCoreApplication::translate("FlrigControl", "connecting to flrig at %1…").arg(a);
    emit changed();
    // I modi della radio: servono per tradurre quelli di Hamlib.
    call(QStringLiteral("rig.get_modes"), {}, [this](const QVariant& v, const QString&) {
        m_modes.clear();
        for (const QVariant& x : v.toList())
            m_modes << x.toString();
    });
    refresh();
    m_poll.start();
}

void FlrigControl::disconnectFromRig()
{
    m_poll.stop();
    m_active = false;
    m_queue.clear();
    const bool was = m_connected;
    m_connected = false;
    m_hz = 0;
    m_mode.clear();
    m_status = QCoreApplication::translate("FlrigControl", "not connected");
    if (was)
        emit changed();
}

void FlrigControl::refresh()
{
    if (!m_active)
        return;
    // Una lettura alla volta: se la precedente e' ancora in coda, si salta.
    for (const Call& c : std::as_const(m_queue))
        if (c.method == QLatin1String("rig.get_vfo"))
            return;
    call(QStringLiteral("rig.get_vfo"), {}, [this](const QVariant& v, const QString& fault) {
        if (!fault.isEmpty())
            return;
        const qint64 hz = v.toString().toLongLong();
        if (hz > 0 && hz != m_hz) {
            m_hz = hz;
            emit changed();
        }
    });
    call(QStringLiteral("rig.get_mode"), {}, [this](const QVariant& v, const QString& fault) {
        if (!fault.isEmpty())
            return;
        const QString mode = flrig::toHamlibMode(v.toString());
        if (mode != m_mode) {
            m_mode = mode;
            emit changed();
        }
    });
    // Split, VFO B e A/B una volta ogni tre giri.
    if (m_extraPoll++ % 3 != 0)
        return;
    call(QStringLiteral("rig.get_split"), {}, [this](const QVariant& v, const QString& fault) {
        if (!fault.isEmpty())
            return;
        const bool on = v.toInt() != 0;
        if (on != m_split) {
            m_split = on;
            emit changed();
        }
    });
    call(QStringLiteral("rig.get_vfoB"), {}, [this](const QVariant& v, const QString& fault) {
        if (!fault.isEmpty())
            return;
        const qint64 hz = v.toString().toLongLong();
        if (hz > 0 && hz != m_vfoB) {
            m_vfoB = hz;
            emit changed();
        }
    });
    call(QStringLiteral("rig.get_AB"), {}, [this](const QVariant& v, const QString& fault) {
        if (!fault.isEmpty())
            return;
        const QString ab = v.toString().trimmed().toUpper() == QLatin1String("B") ? QStringLiteral("VFOB")
                                                                                   : QStringLiteral("VFOA");
        if (ab != m_ab) {
            m_ab = ab;
            emit changed();
        }
    });
}

void FlrigControl::setSplit(bool on, qint64 txHz)
{
    if (on && txHz > 0) {
        call(QStringLiteral("rig.set_vfoB"), {double(txHz)});
        m_vfoB = txHz;
    }
    call(QStringLiteral("rig.set_split"), {on ? 1 : 0});
    m_split = on;
    emit changed();
}

void FlrigControl::setVfo(const QString& vfo)
{
    const bool b = vfo.trimmed().toUpper() == QLatin1String("VFOB");
    call(QStringLiteral("rig.set_AB"), {b ? QStringLiteral("B") : QStringLiteral("A")});
    m_ab = b ? QStringLiteral("VFOB") : QStringLiteral("VFOA");
    emit changed();
}

void FlrigControl::setFrequency(qint64 hz)
{
    if (hz <= 0)
        return;
    call(QStringLiteral("rig.set_vfo"), {double(hz)});
    m_hz = hz;
    emit changed();
}

void FlrigControl::setMode(const QString& mode)
{
    if (mode.isEmpty())
        return;
    call(QStringLiteral("rig.set_mode"), {flrig::fromHamlibMode(mode, m_modes)});
}

void FlrigControl::setPtt(bool on)
{
    call(QStringLiteral("rig.set_ptt"), {on ? 1 : 0});
}

void FlrigControl::setSpeedWpm(int wpm)
{
    m_wpm = wpm;
    call(QStringLiteral("rig.cwio_set_wpm"), {wpm});
}

void FlrigControl::sendMorse(const QString& text)
{
    // flrig manda il CW con il suo keyer (cwio): testo, poi via.
    call(QStringLiteral("rig.cwio_text"), {text}, [this, text](const QVariant&, const QString& fault) {
        if (!fault.isEmpty()) {
            emit morseUnsupported();
            return;
        }
        call(QStringLiteral("rig.cwio_send"), {1});
        emit morseSent(text);
    });
}

void FlrigControl::stopMorse()
{
    call(QStringLiteral("rig.cwio_send"), {0});
}

void FlrigControl::call(const QString& method, const QVariantList& params,
                        std::function<void(const QVariant&, const QString&)> done)
{
    if (!m_active)
        return;
    m_queue << Call{method, params, std::move(done)};
    pump();
}

void FlrigControl::pump()
{
    if (m_busy || m_queue.isEmpty() || !m_active)
        return;
    m_busy = true;
    const Call c = m_queue.takeFirst();
    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml"));
    request.setTransferTimeout(3000);
    QNetworkReply* reply = m_net->post(request, flrig::request(c.method, c.params));
    connect(reply, &QNetworkReply::finished, this, [this, reply, c] {
        reply->deleteLater();
        m_busy = false;
        if (!m_active)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            // Tre errori di fila: flrig non c'e'. Si continua a provare piano.
            if (++m_failures >= 3 && m_connected) {
                m_connected = false;
                m_status = QCoreApplication::translate("FlrigControl", "flrig does not answer: %1").arg(reply->errorString());
                emit failed(m_status);
                emit changed();
            } else if (!m_connected) {
                m_status = QCoreApplication::translate("FlrigControl", "flrig does not answer: %1").arg(reply->errorString());
                emit changed();
            }
            m_queue.clear();
            return;
        }
        m_failures = 0;
        if (!m_connected) {
            m_connected = true;
            m_status = QCoreApplication::translate("FlrigControl", "flrig at %1").arg(m_url.authority());
            emit changed();
        }
        QString fault;
        const QVariant value = flrig::parseResponse(reply->readAll(), &fault);
        if (c.done)
            c.done(value, fault);
        pump();
    });
}

} // namespace decolog::core
