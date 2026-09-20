#include "core/Solar.h"

#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>

namespace decolog::core {

QVariantMap SolarData::toMap() const
{
    auto rows = [](const QList<BandCondition>& list) {
        QVariantList out;
        for (const BandCondition& c : list) {
            out << QVariantMap{{QStringLiteral("band"), c.band},
                               {QStringLiteral("when"), c.when},
                               {QStringLiteral("condition"), c.condition},
                               {QStringLiteral("class"), solar::conditionClass(c.condition)}};
        }
        return out;
    };
    return QVariantMap{
        {QStringLiteral("valid"), valid},
        {QStringLiteral("updated"), updated},
        {QStringLiteral("source"), source},
        {QStringLiteral("fetchedAt"), fetchedAt.isValid() ? fetchedAt.toString(Qt::ISODate) : QString()},
        {QStringLiteral("solarFlux"), solarFlux},
        {QStringLiteral("aIndex"), aIndex},
        {QStringLiteral("kIndex"), kIndex},
        {QStringLiteral("sunspots"), sunspots},
        {QStringLiteral("aurora"), aurora},
        {QStringLiteral("xray"), xray},
        {QStringLiteral("geomagField"), geomagField},
        {QStringLiteral("signalNoise"), signalNoise},
        {QStringLiteral("solarWind"), solarWind},
        {QStringLiteral("magneticField"), magneticField},
        {QStringLiteral("muf"), muf},
        {QStringLiteral("protonFlux"), protonFlux},
        {QStringLiteral("electronFlux"), electronFlux},
        {QStringLiteral("hf"), rows(hf)},
        {QStringLiteral("vhf"), rows(vhf)},
    };
}

namespace solar {

QString conditionClass(const QString& condition)
{
    const QString c = condition.trimmed().toLower();
    if (c.startsWith(QLatin1String("good")) || c.contains(QLatin1String("open")))
        return QStringLiteral("good");
    if (c.startsWith(QLatin1String("fair")))
        return QStringLiteral("fair");
    if (c.startsWith(QLatin1String("poor")))
        return QStringLiteral("poor");
    if (c.contains(QLatin1String("closed")))
        return QStringLiteral("closed");
    return QStringLiteral("unknown");
}

SolarData parse(const QByteArray& xml)
{
    SolarData data;
    QXmlStreamReader r(xml);
    bool inHf = false;
    bool inVhf = false;

    while (!r.atEnd() && !r.hasError()) {
        const auto token = r.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const QString name = r.name().toString().toLower();
            if (name == QLatin1String("calculatedconditions")) {
                inHf = true;
                continue;
            }
            if (name == QLatin1String("calculatedvhfconditions")) {
                inVhf = true;
                continue;
            }
            if (name == QLatin1String("band") && inHf) {
                BandCondition c;
                c.band = r.attributes().value(QLatin1String("name")).toString();
                c.when = r.attributes().value(QLatin1String("time")).toString();
                c.condition = r.readElementText().trimmed();
                if (!c.band.isEmpty())
                    data.hf << c;
                continue;
            }
            if (name == QLatin1String("phenomenon") && inVhf) {
                BandCondition c;
                c.band = r.attributes().value(QLatin1String("name")).toString();
                c.when = r.attributes().value(QLatin1String("location")).toString();
                c.condition = r.readElementText().trimmed();
                if (!c.band.isEmpty())
                    data.vhf << c;
                continue;
            }

            if (name == QLatin1String("source")) {
                data.source = r.readElementText().trimmed();
            } else if (name == QLatin1String("updated")) {
                data.updated = r.readElementText().trimmed();
            } else if (name == QLatin1String("solarflux")) {
                data.solarFlux = r.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("aindex")) {
                data.aIndex = r.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("kindex")) {
                data.kIndex = r.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("sunspots")) {
                data.sunspots = r.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("aurora")) {
                data.aurora = r.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("xray")) {
                data.xray = r.readElementText().trimmed();
            } else if (name == QLatin1String("geomagfield")) {
                data.geomagField = r.readElementText().trimmed();
            } else if (name == QLatin1String("signalnoise")) {
                data.signalNoise = r.readElementText().trimmed();
            } else if (name == QLatin1String("solarwind")) {
                data.solarWind = r.readElementText().trimmed();
            } else if (name == QLatin1String("magneticfield")) {
                data.magneticField = r.readElementText().trimmed();
            } else if (name == QLatin1String("muf")) {
                data.muf = r.readElementText().trimmed();
            } else if (name == QLatin1String("protonflux")) {
                data.protonFlux = r.readElementText().trimmed();
            } else if (name == QLatin1String("electonflux") || name == QLatin1String("electronflux")) {
                // La fonte scrive "electonflux": si accettano tutti e due.
                data.electronFlux = r.readElementText().trimmed();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString name = r.name().toString().toLower();
            if (name == QLatin1String("calculatedconditions"))
                inHf = false;
            else if (name == QLatin1String("calculatedvhfconditions"))
                inVhf = false;
        }
    }

    data.valid = !r.hasError() && (data.solarFlux > 0 || !data.hf.isEmpty());
    if (data.valid)
        data.fetchedAt = QDateTime::currentDateTimeUtc();
    return data;
}

} // namespace solar

SolarFetcher::SolarFetcher(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void SolarFetcher::fetch()
{
    if (m_busy)
        return;
    m_busy = true;
    QNetworkRequest request(m_url);
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(20'000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(network::safeErrorString(reply));
            return;
        }
        const SolarData data = solar::parse(reply->readAll());
        if (!data.valid) {
            emit failed(tr("The solar data cannot be read"));
            return;
        }
        emit finished(data);
    });
}

} // namespace decolog::core
