// DecoLog — invio dei QSO ai servizi QSL.
//
// LoTW non si carica con una password: i QSO vanno firmati con il certificato
// dell'operatore, e l'unico programma che lo sa fare e' TQSL. DecoLog gli passa un
// file ADIF e legge il codice d'uscita, cosi' il certificato resta dove l'ARRL lo
// vuole e non passa mai da qui. QRZ Logbook ed eQSL invece hanno una API e si
// caricano un QSO alla volta.
//
// Ogni invio dice quanti QSO sono stati accettati, quanti erano gia' li' e quanti
// il servizio ha rifiutato: un duplicato non e' un errore, e non va ritentato
// all'infinito.
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

namespace decolog::core {

struct QslUploadResult {
    bool    ok{false};
    int     accepted{0};
    int     duplicates{0};
    int     rejected{0};
    QString message;
    QString remoteId;       // QRZ: il numero del QSO nel logbook
    bool    retryLater{false};   // problema di rete o servizio giu': si riprova
};

namespace qsl {

// Il TQSL installato: dal registro di Windows, dalle cartelle solite o dal PATH.
QString findTqsl();
// I nomi delle "station location" configurate in TQSL (station_data).
QStringList tqslStationLocations();
// Dove TQSL tiene i suoi dati; vuota se non c'e'.
QString tqslDataDirectory();
// TQSL ha un certificato installato?
bool tqslHasCertificate();

// Il codice d'uscita di TQSL tradotto in risultato.
QslUploadResult resultFromTqslExit(int exitCode, const QString& output, int qsoCount);
// La risposta di logbook.qrz.com (RESULT=OK&LOGID=...).
QslUploadResult parseQrzResponse(const QByteArray& body);
// La pagina di eQSL dopo importADIF.cfm.
QslUploadResult parseEqslResponse(const QByteArray& body);
// La risposta di Club Log: `status` e' il codice HTTP, 0 se non e' arrivato.
QslUploadResult parseClubLogResponse(int status, const QByteArray& body, int qsoCount);

} // namespace qsl

// Quello che Club Log vuole sapere a ogni invio. La chiave API e' della
// applicazione, la password e' dell'operatore: due cose diverse.
struct ClubLogAuth {
    QString email;
    QString password;
    QString callsign;
    QString apiKey;

    bool complete() const
    {
        return !email.isEmpty() && !password.isEmpty() && !callsign.isEmpty() && !apiKey.isEmpty();
    }
};

// Manda un file ADIF a LoTW facendolo firmare a TQSL.
class TqslUploader : public QObject {
    Q_OBJECT

public:
    explicit TqslUploader(QObject* parent = nullptr);

    void setProgram(const QString& path) { m_program = path; }
    QString program() const { return m_program; }
    bool busy() const { return m_busy; }

    // `adifPath` e' un file temporaneo con i QSO da caricare; `location` e' la
    // station location di TQSL (vuota: quella predefinita, se TQSL ne ha una sola).
    void upload(const QString& adifPath, const QString& location, int qsoCount);
    void cancel();

signals:
    void finished(const decolog::core::QslUploadResult& result);

private:
    QString m_program;
    bool    m_busy{false};
    QProcess* m_process{nullptr};
};

// QRZ Logbook ed eQSL: un QSO per volta, con la chiave o la password del servizio.
class WebQslUploader : public QObject {
    Q_OBJECT

public:
    enum class Service { QrzLogbook, Eqsl, ClubLog };

    explicit WebQslUploader(QObject* parent = nullptr);

    // Per i test: server finti al posto di logbook.qrz.com e www.eqsl.cc.
    void setEndpoints(const QUrl& qrz, const QUrl& eqsl);
    void setClubLogEndpoints(const QUrl& realtime, const QUrl& batch);
    bool busy() const { return m_busy; }

    // `credentials`: per QRZ la chiave API; per eQSL utente e password.
    void uploadQrz(const QString& apiKey, const QString& adifRecord);
    void uploadEqsl(const QString& user, const QString& password, const QString& adifRecord);
    // Club Log prende tutto il blocco in una volta: un QSO solo passa da
    // realtime.php, il resto dal caricamento normale di un file ADIF.
    void uploadClubLog(const ClubLogAuth& auth, const QByteArray& adifDocument, int qsoCount);

signals:
    void finished(const decolog::core::QslUploadResult& result);

private:
    void send(Service service, const QUrl& url, const QByteArray& body);
    void watch(QNetworkReply* reply, Service service, int qsoCount);

    QNetworkAccessManager* m_net;
    QUrl m_qrzUrl{QStringLiteral("https://logbook.qrz.com/api")};
    QUrl m_eqslUrl{QStringLiteral("https://www.eqsl.cc/qslcard/importADIF.cfm")};
    QUrl m_clubLogRealtimeUrl{QStringLiteral("https://clublog.org/realtime.php")};
    QUrl m_clubLogBatchUrl{QStringLiteral("https://clublog.org/putlogs.php")};
    bool m_busy{false};
};

} // namespace decolog::core
