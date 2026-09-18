// DecoLog — le condizioni del Sole e delle bande.
//
// I numeri (SFI, A, K, macchie, aurora) e le condizioni banda per banda arrivano
// dal XML di N0NBH (hamqsl.com), che e' quello che sta nei banner di mezzo mondo.
// Si scarica ogni tanto, non di continuo: cambia ogni ora e non serve a nessuno
// tempestare un sito di richieste.
//
// Lo storico serve a rispondere alla domanda vera: "con che condizioni ho fatto
// piu' QSO?" — per quello basta tenere un campione all'ora.
#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;

namespace decolog::core {

// Una riga delle condizioni: "30m-20m" di giorno = Good.
struct BandCondition {
    QString band;       // 80m-40m, 30m-20m, 17m-15m, 12m-10m
    QString when;       // day | night, oppure la zona per il VHF
    QString condition;  // Good | Fair | Poor | Band Closed...
};

struct SolarData {
    bool      valid{false};
    QDateTime fetchedAt;
    QString   updated;          // come lo scrive la fonte: "18 Sep 2026 0730 GMT"
    QString   source;
    int       solarFlux{0};
    int       aIndex{0};
    int       kIndex{0};
    int       sunspots{0};
    int       aurora{0};
    QString   xray;
    QString   geomagField;
    QString   signalNoise;
    QString   solarWind;
    QString   magneticField;
    QString   muf;
    QString   protonFlux;
    QString   electronFlux;
    QList<BandCondition> hf;
    QList<BandCondition> vhf;

    QVariantMap toMap() const;
};

namespace solar {

// Legge il XML di hamqsl.com. Torna un dato non valido se non si capisce.
SolarData parse(const QByteArray& xml);
// Il colore che merita una condizione: good | fair | poor | closed | unknown.
QString conditionClass(const QString& condition);

} // namespace solar

// Scarica il XML. Un errore non e' mai fatale: si riprova al giro dopo.
class SolarFetcher : public QObject {
    Q_OBJECT

public:
    explicit SolarFetcher(QObject* parent = nullptr);

    void setUrl(const QUrl& url) { m_url = url; }
    bool busy() const { return m_busy; }
    void fetch();

signals:
    void finished(const decolog::core::SolarData& data);
    void failed(const QString& error);

private:
    QNetworkAccessManager* m_net;
    QUrl m_url{QStringLiteral("https://www.hamqsl.com/solarxml.php")};
    bool m_busy{false};
};

} // namespace decolog::core
