// DecoLog — callbook QRZ.com (XML) e HamQTH.
//
// Una ricerca per nominativo: nome, QTH, locatore, zone, foto. Le credenziali
// arrivano dal portachiavi solo al momento del login; la chiave di sessione resta
// in memoria e si rinnova da sola quando il servizio la dichiara scaduta. I
// risultati restano in memoria per un giorno, cosi' passare avanti e indietro fra
// le righe del log non consuma ricerche dell'abbonamento.
#pragma once

#include "core/Adif.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <functional>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace decolog::core {

struct CallbookRecord {
    QString call;
    QString name;
    QString qth;
    QString address;
    QString state;
    QString county;
    QString country;
    QString grid;
    QString iota;
    QString email;
    QString qslVia;
    QString imageUrl;
    int     dxcc{0};
    int     cqZone{0};
    int     ituZone{0};
    double  lat{0.0};
    double  lon{0.0};
    bool    hasPosition{false};
    bool    lotw{false};
    bool    eqsl{false};
    QString source;         // "QRZ.com" o "HamQTH"

    QVariantMap toMap() const;
};

// Il parsing e' separato dalla rete: si prova sui documenti XML dei servizi.
namespace callbook {

struct SessionResult {
    QString key;            // vuota se il login non e' riuscito
    QString error;
    bool expired{false};    // la chiave usata non vale piu': rifare il login
};

SessionResult parseQrzSession(const QByteArray& xml);
std::optional<CallbookRecord> parseQrzCallsign(const QByteArray& xml);
SessionResult parseHamQthSession(const QByteArray& xml);
std::optional<CallbookRecord> parseHamQthSearch(const QByteArray& xml);

// Completa un QSO con quello che il callbook sa di quel nominativo — nome,
// citta', indirizzo, locatore, zone — e torna i campi che ha riempito.
//
// **Solo i campi vuoti.** Quello che c'e' nel QSO l'ha messo l'operatore o l'ha
// detto la radio: ha visto il collegamento, il callbook no. Un locatore scritto
// a mano vale piu' di uno trovato su Internet, e il DXCC del cty.csv vale piu'
// di quello che dichiara una scheda personale.
QStringList fillMissing(AdifRecord& record, const CallbookRecord& found);

} // namespace callbook

class CallbookClient : public QObject {
    Q_OBJECT

public:
    enum class Provider { None, Qrz, HamQth };

    // Legge il segreto di un servizio ("qrz", "hamqth"); risponde nella callback.
    using SecretReader = std::function<void(const QString& service,
                                            std::function<void(const QString& secret, const QString& error)>)>;
    using AccountReader = std::function<QString(const QString& service)>;

    explicit CallbookClient(QObject* parent = nullptr);

    void setProvider(Provider provider);
    Provider provider() const { return m_provider; }
    static QString providerId(Provider p);
    static Provider providerFromId(const QString& id);

    void setCredentialReaders(AccountReader accounts, SecretReader secrets);
    // Per i test: server finti al posto di xmldata.qrz.com e hamqth.com.
    void setEndpoints(const QUrl& qrz, const QUrl& hamqth);

    // Cerca un nominativo. Il risultato arriva con found() o failed(); dalla cache
    // arriva subito, nella stessa chiamata.
    void lookup(const QString& callsign);
    // Dimentica sessione e cache (credenziali cambiate).
    void reset();

signals:
    void found(const QString& callsign, const decolog::core::CallbookRecord& record);
    void failed(const QString& callsign, const QString& message);

private:
    void login(std::function<void(const QString& error)> done);
    void query(const QString& callsign, bool retried);
    QString serviceId() const;
    QString sourceName() const;

    QNetworkAccessManager* m_net;
    Provider m_provider{Provider::None};
    AccountReader m_accounts;
    SecretReader m_secrets;
    QUrl m_qrzUrl{QStringLiteral("https://xmldata.qrz.com/xml/current/")};
    QUrl m_hamqthUrl{QStringLiteral("https://www.hamqth.com/xml.php")};
    QString m_sessionKey;
    QHash<QString, QPair<QDateTime, CallbookRecord>> m_cache;
    QHash<QString, QDateTime> m_notFound;
};

} // namespace decolog::core
