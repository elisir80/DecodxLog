// DecoDXLog — le conferme che arrivano da eQSL e da QRZ.
//
// LoTW ha il suo scarico (Lotw.h). Qui gli altri due servizi che confermano:
//
//  - eQSL: DownloadInBox.cfm prepara un file ADIF con le eQSL ricevute e
//    risponde con una pagina HTML che contiene il link al file ("Your ADIF log
//    file has been built"). Si scarica il file, e ogni eQSL ricevuta e' una
//    conferma del QSO con quel nominativo, a quell'ora, su quella banda.
//  - QRZ Logbook: l'API (ACTION=FETCH, OPTION=STATUS:CONFIRMED) restituisce i
//    QSO del proprio logbook su QRZ che l'altra stazione ha confermato
//    (APP_QRZLOG_STATUS = C), a pagine, con l'ADIF scritto come &lt;call:4&gt;.
//
// In tutti e due i casi le conferme si segnano con LogDatabase::applyConfirmation,
// come quelle di LoTW: stesso nominativo, stessa banda, stesso gruppo di modi,
// entro mezz'ora.
#pragma once

#include "core/Adif.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace decolog::core {

namespace confirmations {

struct Report {
    bool ok{false};
    QString service;             // "eqsl" o "qrz"
    QString error;               // gia' tradotto
    QList<AdifRecord> confirmations;   // con QSLRDATE, pronte per applyConfirmation
};

// eQSL: dalla pagina di DownloadInBox il link al file .adi. Vuoto, con
// l'errore scritto dalla pagina, se il file non c'e'.
QUrl eqslFileLink(const QByteArray& html, const QUrl& page, QString* error);

// Le eQSL ricevute del file .adi, come conferme (QSLRDATE = EQSL_QSLRDATE).
// Le segnalazioni SWL non sono QSO e restano fuori.
QList<AdifRecord> eqslConfirmations(const QByteArray& adif);

// QRZ: una pagina di risposta di FETCH.
struct QrzPage {
    bool ok{false};
    QString error;
    int count{0};
    QList<AdifRecord> records;
    qint64 lastLogId{0};         // il piu' alto APP_QRZLOG_LOGID della pagina
};
QrzPage parseQrzFetch(const QByteArray& body);

// Solo i QSO confermati (APP_QRZLOG_STATUS = C), come conferme
// (QSLRDATE = APP_QRZLOG_QSLDATE).
QList<AdifRecord> qrzConfirmations(const QList<AdifRecord>& records);

} // namespace confirmations

class ConfirmationDownloader : public QObject {
    Q_OBJECT

public:
    explicit ConfirmationDownloader(QObject* parent = nullptr);

    // Per le prove: server finti al posto di www.eqsl.cc e logbook.qrz.com.
    void setEndpoints(const QUrl& eqslInbox, const QUrl& qrzApi);
    bool busy() const { return m_reply != nullptr; }

    // Le eQSL ricevute da `since` (vuoto = tutte).
    void downloadEqsl(const QString& user, const QString& password, const QDateTime& since = {});
    // Le conferme di QRZ dei QSO modificati da `since` (non valida = tutte).
    void downloadQrz(const QString& apiKey, const QDate& since = {});
    void cancel();

    static constexpr int kQrzPage = 250;

signals:
    void finished(const decolog::core::confirmations::Report& report);

private:
    void finish(confirmations::Report report);
    void fetchQrzPage();

    QNetworkAccessManager* m_net;
    QNetworkReply* m_reply{nullptr};
    QUrl m_eqslUrl{QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInBox.cfm")};
    QUrl m_qrzUrl{QStringLiteral("https://logbook.qrz.com/api")};
    // Lo scarico di QRZ in corso, pagina dopo pagina.
    QString m_qrzKey;
    QDate m_qrzSince;
    qint64 m_qrzAfter{0};
    int m_qrzPages{0};
    QList<AdifRecord> m_qrzConfirmations;
};

} // namespace decolog::core
