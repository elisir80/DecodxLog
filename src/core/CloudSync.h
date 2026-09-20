// DecoDXLog — il cliente del DecoDXLog Cloud.
//
// Parla con il servizio di `server/`: token, spinta dei QSO in coda, ripresa di
// quello che e' cambiato altrove. Qui dentro non c'e' logica di log: si manda
// quello che il database dice di mandare e si riporta quello che il server
// risponde, uno a uno, perche' ogni QSO ha il suo esito.
#pragma once

#include <QDateTime>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace decolog::core {

struct CloudError {
    bool    ok{true};
    bool    retryLater{false};   // rete giu' o servizio occupato: si riprova
    bool    unauthorized{false}; // token scaduto o password cambiata
    QString message;
};

namespace cloudsync {

// Il motivo di un errore, comunque il server lo scriva.
//
// I server di DecoDXLog mettono una frase in `detail`. Ma FastAPI, quando la
// richiesta non passa la validazione, mette li' un *elenco* di oggetti
// ({"loc": [...], "msg": "..."}): chi si aspettava una stringa restava con un
// "HTTP 422" in mano e nessuna idea di cosa avesse sbagliato. Qui si legge
// anche quella forma.
QString detailOf(const QJsonValue& value);

} // namespace cloudsync

class CloudSync : public QObject {
    Q_OBJECT

public:
    explicit CloudSync(QObject* parent = nullptr);

    void setServer(const QUrl& base) { m_base = base; }
    QUrl server() const { return m_base; }
    void setToken(const QString& token) { m_token = token; }
    QString token() const { return m_token; }
    void setDevice(const QString& device) { m_device = device; }
    QString device() const { return m_device; }
    bool busy() const { return m_busy; }

    // Nominativo e password -> token. `signup` crea l'account, `login` no.
    void signup(const QString& callsign, const QString& password);
    void login(const QString& callsign, const QString& password);
    // Dov'e' la stazione adesso: frequenza, banda, modo, se sta trasmettendo.
    // Non e' log — non ha revisione, non ha storia, non muove il cursore — e un
    // errore qui non si racconta: al giro dopo si riprova.
    void reportPresence(const QVariantMap& state);
    // I QSO in coda e i documenti (profili, impostazioni), gia' pronti come li
    // vuole il server.
    void push(const QVariantList& qsos, const QVariantList& docs = {});
    // Quello che e' cambiato dopo `since`.
    void pull(qint64 since, int limit = 0);
    void status();
    // Svuota il Cloud di questo nominativo. `confirm` deve essere la parola
    // DELETE: il server non si fida di un clic, e nemmeno noi.
    void purge(const QString& confirm);
    void cancel();

signals:
    void loggedIn(const QString& token, const QString& callsign);
    // Un esito per QSO: {uuid, status, revision, seq, serverUuid}; e uno per
    // documento: {kind, key, status, revision}.
    void pushed(const QVariantList& results, const QVariantList& docResults, qint64 cursor);
    void pulled(const QVariantList& qsos, const QVariantList& docs, qint64 cursor, bool more);
    void statusReady(const QVariantMap& status);
    // Il Cloud e' stato svuotato: quanti QSO, documenti e storie se ne sono andati.
    void purged(const QVariantMap& deleted);
    void failed(const decolog::core::CloudError& error);

private:
    QNetworkReply* send(const QString& path, const QVariantMap& body, bool authenticated);
    QNetworkReply* get(const QString& path, const QVariantMap& query);
    void watch(QNetworkReply* reply, const QString& what);

    QNetworkAccessManager* m_net;
    QUrl    m_base;
    QString m_token;
    QString m_device;
    bool    m_busy{false};
};

} // namespace decolog::core
