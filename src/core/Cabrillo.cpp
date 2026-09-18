#include "core/Cabrillo.h"

#include <QCoreApplication>
#include <QDateTime>

namespace decolog::core::cabrillo {

namespace {

QString value(const AdifRecord& r, const char* name)
{
    return r.value(QLatin1String(name)).trimmed();
}

// Cabrillo vuole il nominativo a sinistra dentro tredici colonne e lo scambio
// dentro sei: piu' lungo di cosi' si taglia, perche' una colonna in piu' sposta
// tutto il resto.
QString pad(const QString& text, int width)
{
    return text.left(width).leftJustified(width, QLatin1Char(' '));
}

QString padRight(const QString& text, int width)
{
    return text.left(width).rightJustified(width, QLatin1Char(' '));
}

void addLine(QByteArray& out, const QString& keyword, const QString& text)
{
    if (text.trimmed().isEmpty())
        return;
    out += (keyword + QStringLiteral(": ") + text.trimmed() + QLatin1Char('\n')).toUtf8();
}

} // namespace

QString modeCode(const QString& mode, const QString& submode)
{
    const QString m = mode.trimmed().toUpper();
    const QString s = submode.trimmed().toUpper();
    if (m == QLatin1String("CW"))
        return QStringLiteral("CW");
    if (m == QLatin1String("SSB") || m == QLatin1String("AM"))
        return QStringLiteral("PH");
    if (m == QLatin1String("FM"))
        return QStringLiteral("FM");
    if (m == QLatin1String("RTTY") || s == QLatin1String("RTTY"))
        return QStringLiteral("RY");
    if (m.isEmpty())
        return QStringLiteral("DG");
    // FT2, FT8, FT4, PSK, MFSK e tutto il resto: digitale.
    return QStringLiteral("DG");
}

QString frequencyField(const QString& freqMhz, const QString& band)
{
    bool ok = false;
    const double mhz = freqMhz.trimmed().toDouble(&ok);
    // Sopra i 50 MHz Cabrillo non vuole la frequenza ma il nome della banda.
    const QString b = band.trimmed().toLower();
    static const QList<QPair<QString, QString>> vhf{
        {QStringLiteral("6m"), QStringLiteral("50")},     {QStringLiteral("4m"), QStringLiteral("70")},
        {QStringLiteral("2m"), QStringLiteral("144")},    {QStringLiteral("1.25m"), QStringLiteral("222")},
        {QStringLiteral("70cm"), QStringLiteral("432")},  {QStringLiteral("33cm"), QStringLiteral("902")},
        {QStringLiteral("23cm"), QStringLiteral("1.2G")}, {QStringLiteral("13cm"), QStringLiteral("2.3G")},
        {QStringLiteral("9cm"), QStringLiteral("3.4G")},  {QStringLiteral("6cm"), QStringLiteral("5.7G")},
        {QStringLiteral("3cm"), QStringLiteral("10G")},
    };
    for (const auto& pair : vhf) {
        if (b == pair.first)
            return pair.second;
    }
    if (ok && mhz > 0.0)
        return QString::number(qRound(mhz * 1000.0));
    return QStringLiteral("0");
}

QString qsoLine(const Info& info, const AdifRecord& r)
{
    const QString call = value(r, "CALL").toUpper();
    const QString date = value(r, "QSO_DATE");
    const QString time = value(r, "TIME_ON");
    if (call.isEmpty() || date.size() < 8 || time.size() < 4)
        return {};

    const QString myCall = info.callsign.toUpper();
    // Lo scambio mandato: il numero progressivo se c'e', altrimenti la sezione o
    // la zona della testata.
    QString sent = value(r, "STX_STRING");
    if (sent.isEmpty())
        sent = value(r, "STX");
    if (sent.isEmpty())
        sent = info.location;
    QString rcvd = value(r, "SRX_STRING");
    if (rcvd.isEmpty())
        rcvd = value(r, "SRX");

    QString rstSent = value(r, "RST_SENT");
    QString rstRcvd = value(r, "RST_RCVD");
    const QString mode = modeCode(value(r, "MODE"), value(r, "SUBMODE"));
    if (rstSent.isEmpty())
        rstSent = mode == QLatin1String("PH") ? QStringLiteral("59") : QStringLiteral("599");
    if (rstRcvd.isEmpty())
        rstRcvd = mode == QLatin1String("PH") ? QStringLiteral("59") : QStringLiteral("599");

    const QString day = date.left(4) + QLatin1Char('-') + date.mid(4, 2) + QLatin1Char('-') + date.mid(6, 2);
    const QStringList fields{padRight(frequencyField(value(r, "FREQ"), value(r, "BAND")), 5),
                             pad(mode, 2), day, time.left(4),
                             pad(myCall, 13), pad(rstSent, 3), pad(sent, 6),
                             pad(call, 13), pad(rstRcvd, 3), pad(rcvd, 6)};
    return QStringLiteral("QSO: ") + fields.join(QLatin1Char(' '));
}

QByteArray write(const Info& info, const QList<AdifRecord>& qsos, QString* error)
{
    if (info.callsign.trimmed().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Cabrillo", "The station callsign is missing");
        return {};
    }
    if (info.contest.trimmed().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Cabrillo", "The contest name is missing");
        return {};
    }

    QByteArray out;
    addLine(out, QStringLiteral("START-OF-LOG"), QStringLiteral("3.0"));
    addLine(out, QStringLiteral("CONTEST"), info.contest.toUpper());
    addLine(out, QStringLiteral("CALLSIGN"), info.callsign.toUpper());
    addLine(out, QStringLiteral("CATEGORY-OPERATOR"), info.categoryOperator);
    addLine(out, QStringLiteral("CATEGORY-ASSISTED"), info.categoryAssisted);
    addLine(out, QStringLiteral("CATEGORY-BAND"), info.categoryBand);
    addLine(out, QStringLiteral("CATEGORY-MODE"), info.categoryMode);
    addLine(out, QStringLiteral("CATEGORY-POWER"), info.categoryPower);
    addLine(out, QStringLiteral("CATEGORY-TRANSMITTER"), info.categoryTransmitter);
    addLine(out, QStringLiteral("CATEGORY-OVERLAY"), info.categoryOverlay);
    addLine(out, QStringLiteral("GRID-LOCATOR"), info.gridLocator.toUpper());
    addLine(out, QStringLiteral("LOCATION"), info.location);
    addLine(out, QStringLiteral("CLAIMED-SCORE"), info.claimedScore > 0 ? QString::number(info.claimedScore) : QString());
    addLine(out, QStringLiteral("CLUB"), info.club);
    addLine(out, QStringLiteral("NAME"), info.name);
    for (const QString& line : info.address)
        addLine(out, QStringLiteral("ADDRESS"), line);
    addLine(out, QStringLiteral("EMAIL"), info.email);
    addLine(out, QStringLiteral("OPERATORS"), info.operators.toUpper());
    addLine(out, QStringLiteral("CREATED-BY"),
            QStringLiteral("DecoLog %1").arg(QCoreApplication::applicationVersion()));
    for (const QString& line : info.soapbox)
        addLine(out, QStringLiteral("SOAPBOX"), line);

    int written = 0;
    for (const AdifRecord& r : qsos) {
        const QString line = qsoLine(info, r);
        if (line.isEmpty())
            continue;
        out += line.toUtf8() + '\n';
        ++written;
    }
    out += QByteArray("END-OF-LOG:\n");

    if (written == 0 && error)
        *error = QCoreApplication::translate("Cabrillo", "No usable QSO");
    return out;
}

} // namespace decolog::core::cabrillo
