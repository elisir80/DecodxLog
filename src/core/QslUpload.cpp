#include "core/QslUpload.h"

#include "core/Adif.h"
#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cmath>
#include <utility>
#include <QUrlQuery>
#include <QXmlStreamReader>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

namespace decolog::core {

namespace qsl {

QString tqslDataDirectory()
{
    QStringList candidates;
#ifdef Q_OS_WIN
    // Su Windows TQSL tiene i suoi dati in %APPDATA%\TrustedQSL — la cartella
    // "Roaming" —: li' stanno il certificato del nominativo, le chiavi e le
    // station location. Prima si guardava solo in "Local", dove non c'e'
    // niente, e cosi' un TQSL a posto sembrava non installato.
    const QString roaming = qEnvironmentVariable("APPDATA");
    if (!roaming.isEmpty())
        candidates << QDir(roaming).filePath(QStringLiteral("TrustedQSL"));
#endif
    candidates << QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                      .filePath(QStringLiteral("TrustedQSL"));
    // Su Linux e macOS TQSL usa la cartella nascosta nella home.
    candidates << QDir(QDir::homePath()).filePath(QStringLiteral(".tqsl"));
    for (const QString& dir : std::as_const(candidates)) {
        if (QFileInfo::exists(dir))
            return dir;
    }
    return {};
}

QString findRigctld()
{
#ifdef Q_OS_WIN
    // Hamlib si installa in "Program Files\hamlib-w64-<versione>": si prende la
    // piu' recente che si trova.
    QStringList roots{QStringLiteral("C:/Program Files"), QStringLiteral("C:/Program Files (x86)")};
    QStringList found;
    for (const QString& root : roots) {
        QDir dir(root);
        for (const QString& name : dir.entryList({QStringLiteral("hamlib*")}, QDir::Dirs)) {
            const QString exe = dir.filePath(name + QStringLiteral("/bin/rigctld.exe"));
            if (QFileInfo::exists(exe))
                found << QDir::toNativeSeparators(exe);
        }
    }
    found.sort();
    if (!found.isEmpty())
        return found.last();
#endif
    return QStandardPaths::findExecutable(QStringLiteral("rigctld"));
}

QString findTqsl()
{
#ifdef Q_OS_WIN
    // TQSL registra dove si e' installato; e' la via piu' affidabile.
    for (const auto scope : {QSettings::NativeFormat}) {
        for (const char* key : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\TrustedQSL",
                                "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\TrustedQSL",
                                "HKEY_CURRENT_USER\\SOFTWARE\\TrustedQSL"}) {
            QSettings reg(QLatin1String(key), scope);
            const QString path = reg.value(QStringLiteral("Path")).toString();
            if (!path.isEmpty()) {
                const QString exe = QDir(path).filePath(QStringLiteral("tqsl.exe"));
                if (QFileInfo::exists(exe))
                    return QDir::toNativeSeparators(exe);
            }
        }
    }
    for (const char* candidate : {"C:/Program Files (x86)/TrustedQSL/tqsl.exe",
                                  "C:/Program Files/TrustedQSL/tqsl.exe"}) {
        if (QFileInfo::exists(QLatin1String(candidate)))
            return QDir::toNativeSeparators(QLatin1String(candidate));
    }
#else
    for (const char* candidate : {"/usr/bin/tqsl", "/usr/local/bin/tqsl",
                                  "/Applications/TrustedQSL/tqsl.app/Contents/MacOS/tqsl"}) {
        if (QFileInfo::exists(QLatin1String(candidate)))
            return QLatin1String(candidate);
    }
#endif
    return QStandardPaths::findExecutable(QStringLiteral("tqsl"));
}

QStringList tqslStationLocations()
{
    QStringList out;
    const QString dir = tqslDataDirectory();
    if (dir.isEmpty())
        return out;
    QFile file(QDir(dir).filePath(QStringLiteral("station_data")));
    if (!file.open(QIODevice::ReadOnly))
        return out;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement
            && xml.name().compare(QLatin1String("StationData"), Qt::CaseInsensitive) == 0) {
            const QString name = xml.attributes().value(QLatin1String("name")).toString();
            if (!name.isEmpty())
                out << name;
        }
    }
    return out;
}

bool tqslHasCertificate()
{
    const QString dir = tqslDataDirectory();
    if (dir.isEmpty())
        return false;
    // In certs/ ci sono sempre "root" e "authorities": quelli li mette TQSL da
    // solo appena installato, e non firmano niente. Il certificato che conta e'
    // quello del nominativo, che sta in "user" e arriva col file .tq6 di ARRL.
    const QFileInfo user(QDir(dir).filePath(QStringLiteral("certs/user")));
    if (user.exists() && user.size() > 0)
        return true;
    const QDir certs(QDir(dir).filePath(QStringLiteral("certs")));
    for (const QString& name : certs.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (name != QLatin1String("root") && name != QLatin1String("authorities"))
            return true;
    }
    return false;
}

QslUploadResult resultFromTqslExit(int exitCode, const QString& output, int qsoCount)
{
    QslUploadResult r;
    r.message = output.trimmed();
    // I codici di TQSL (tqsl --help): 0 tutto caricato, 7 tutti duplicati,
    // 8 caricati con qualche duplicato, 9 nessun QSO nell'intervallo.
    switch (exitCode) {
    case 0:
        r.ok = true;
        r.accepted = qsoCount;
        if (r.message.isEmpty())
            r.message = QCoreApplication::translate("Qsl", "%n QSO sent to LoTW", nullptr, qsoCount);
        break;
    case 7:
        r.ok = true;
        r.duplicates = qsoCount;
        r.message = QCoreApplication::translate("Qsl", "LoTW already had these QSOs");
        break;
    case 8:
        r.ok = true;
        r.accepted = qsoCount;
        r.message = QCoreApplication::translate("Qsl", "Sent to LoTW, some were already there");
        break;
    case 9:
        r.ok = true;
        r.message = QCoreApplication::translate("Qsl", "No QSO to send");
        break;
    case 1:
        r.message = QCoreApplication::translate("Qsl", "TQSL: cancelled");
        break;
    case 2:
        r.rejected = qsoCount;
        r.message = QCoreApplication::translate("Qsl", "LoTW rejected the file: %1").arg(r.message);
        break;
    case 5:
    case 6:
        r.message = QCoreApplication::translate("Qsl", "TQSL: certificate or station location problem (%1)").arg(r.message);
        break;
    case 10:
    case 11:
        r.retryLater = true;
        r.message = QCoreApplication::translate("Qsl", "TQSL: cannot reach LoTW (%1)").arg(r.message);
        break;
    default:
        r.message = QCoreApplication::translate("Qsl", "TQSL: error %1 %2").arg(exitCode).arg(r.message);
        break;
    }
    return r;
}

QslUploadResult parseQrzResponse(const QByteArray& body)
{
    QslUploadResult r;
    const QUrlQuery q(QString::fromUtf8(body).trimmed());
    const QString result = q.queryItemValue(QStringLiteral("RESULT"));
    const QString reason = q.queryItemValue(QStringLiteral("REASON"), QUrl::FullyDecoded);
    if (result == QLatin1String("OK")) {
        r.ok = true;
        r.accepted = 1;
        r.remoteId = q.queryItemValue(QStringLiteral("LOGID"));
        r.message = QCoreApplication::translate("Qsl", "QRZ Logbook: QSO %1").arg(r.remoteId);
        return r;
    }
    if (reason.contains(QLatin1String("duplicate"), Qt::CaseInsensitive)) {
        r.ok = true;
        r.duplicates = 1;
        r.message = QCoreApplication::translate("Qsl", "QRZ Logbook: already there");
        return r;
    }
    if (result.isEmpty()) {
        r.retryLater = true;
        r.message = QCoreApplication::translate("Qsl", "QRZ Logbook: unexpected answer");
        return r;
    }
    r.rejected = 1;
    r.message = QCoreApplication::translate("Qsl", "QRZ Logbook: %1").arg(reason.isEmpty() ? result : reason);
    return r;
}

QJsonObject crxQsoData(const AdifRecord& record, qint64 logId, qint64 remoteId)
{
    // La frequenza in kHz, come nell'esempio della documentazione ("7025").
    const double mhz = record.value(QStringLiteral("FREQ")).toDouble();
    const QString khz = mhz > 0 ? QString::number(mhz * 1000.0, 'f', mhz * 1000.0 == std::floor(mhz * 1000.0) ? 0 : 1)
                                : QString();
    // Il modo come lo scrivono gli operatori: FT8 e non MFSK.
    QString mode = record.value(QStringLiteral("MODE")).toUpper();
    const QString submode = record.value(QStringLiteral("SUBMODE")).toUpper();
    if (!submode.isEmpty() && mode != QLatin1String("SSB"))
        mode = submode;
    // L'ora del QSO, in secondi Unix come la restituisce get_myqsos. La
    // documentazione di edit_myqso non la elenca: senza, CRX metterebbe l'ora
    // dell'invio, e un QSO mandato dopo avrebbe l'ora sbagliata.
    const QString date = record.value(QStringLiteral("QSO_DATE"));
    QString time = record.value(QStringLiteral("TIME_ON"));
    if (time.size() == 4)
        time += QStringLiteral("00");
    const QDateTime when = QDateTime::fromString(date + time, QStringLiteral("yyyyMMddHHmmss"));
    QJsonObject out{
        {QStringLiteral("qso_id"), remoteId},
        {QStringLiteral("f_log_id"), logId},
        {QStringLiteral("logentry_his_call"), record.value(QStringLiteral("CALL")).toUpper()},
        {QStringLiteral("logentry_band"), record.value(QStringLiteral("BAND")).toLower()},
        {QStringLiteral("logentry_frequency"), khz},
        {QStringLiteral("logentry_mode"), mode},
        // "his report" e' il rapporto dato a lui, "my report" quello ricevuto.
        {QStringLiteral("logentry_his_report"), record.value(QStringLiteral("RST_SENT"))},
        {QStringLiteral("logentry_my_report"), record.value(QStringLiteral("RST_RCVD"))},
    };
    if (when.isValid()) {
        QDateTime utc = when;
        utc.setTimeZone(QTimeZone::UTC);
        out.insert(QStringLiteral("logentry_date"), utc.toSecsSinceEpoch());
    }
    const QString name = record.value(QStringLiteral("NAME"));
    if (!name.isEmpty())
        out.insert(QStringLiteral("logentry_his_name"), name);
    const QString comment = record.value(QStringLiteral("COMMENT"));
    if (!comment.isEmpty())
        out.insert(QStringLiteral("logentry_comment"), comment);
    return out;
}

QslUploadResult parseCrxResponse(int status, const QByteArray& body)
{
    QslUploadResult r;
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    if (o.value(QStringLiteral("success")).toBool()) {
        r.ok = true;
        r.accepted = 1;
        const QJsonValue id = o.value(QStringLiteral("qso_id"));
        r.remoteId = id.isString() ? id.toString() : QString::number(id.toVariant().toLongLong());
        r.message = QCoreApplication::translate("Qsl", "CRX Logbook: QSO %1").arg(r.remoteId);
        return r;
    }
    const QString error = o.value(QStringLiteral("error")).toString(o.value(QStringLiteral("message")).toString());
    if (status == 0 || status >= 500 || (status == 200 && o.isEmpty())) {
        // Il servizio non ha risposto, o ha risposto con qualcosa che non e'
        // JSON: si riprova dopo, il QSO resta in coda.
        r.retryLater = true;
        r.message = QCoreApplication::translate("Qsl", "CRX Logbook: no answer from the service (%1)")
                        .arg(status > 0 ? QString::number(status) : QStringLiteral("—"));
        return r;
    }
    if (status == 401) {
        // Una chiave sbagliata vale per tutti i QSO: ci si ferma qui.
        r.retryLater = true;
        r.message = QCoreApplication::translate("Qsl", "CRX Logbook: the API key was not accepted");
        return r;
    }
    if (error.contains(QLatin1String("duplicate"), Qt::CaseInsensitive)) {
        r.ok = true;
        r.duplicates = 1;
        r.message = QCoreApplication::translate("Qsl", "CRX Logbook: already there");
        return r;
    }
    r.rejected = 1;
    r.message = QCoreApplication::translate("Qsl", "CRX Logbook: %1")
                    .arg(error.isEmpty() ? QString::number(status) : error);
    return r;
}

QVariantList parseCrxLogs(const QByteArray& body, QString* error)
{
    QVariantList out;
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    if (o.contains(QStringLiteral("error"))) {
        if (error)
            *error = o.value(QStringLiteral("error")).toString();
        return out;
    }
    for (const QJsonValue& v : o.value(QStringLiteral("logs")).toArray()) {
        const QJsonObject log = v.toObject();
        out << QVariantMap{
            {QStringLiteral("id"), log.value(QStringLiteral("log_id")).toVariant().toLongLong()},
            {QStringLiteral("name"), log.value(QStringLiteral("log_name")).toString()},
            {QStringLiteral("call"), log.value(QStringLiteral("log_activation_call")).toString()},
            {QStringLiteral("description"), log.value(QStringLiteral("log_desc")).toString()},
        };
    }
    if (out.isEmpty() && error && !o.contains(QStringLiteral("logs")))
        *error = QCoreApplication::translate("Qsl", "unexpected answer");
    return out;
}

QslUploadResult parseEqslResponse(const QByteArray& body)
{
    QslUploadResult r;
    const QString text = QString::fromUtf8(body).remove(QRegularExpression(QStringLiteral("<[^>]*>"))).simplified();
    static const QRegularExpression added(QStringLiteral("Result:\\s*(\\d+)\\s*out of\\s*(\\d+)\\s*record"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (const auto m = added.match(text); m.hasMatch()) {
        r.accepted = m.captured(1).toInt();
        r.ok = r.accepted > 0;
        if (!r.ok && text.contains(QLatin1String("Duplicate"), Qt::CaseInsensitive)) {
            r.ok = true;
            r.duplicates = 1;
            r.message = QCoreApplication::translate("Qsl", "eQSL: already there");
            return r;
        }
        r.message = QCoreApplication::translate("Qsl", "eQSL: %1").arg(text.left(120));
        return r;
    }
    if (text.contains(QLatin1String("Duplicate"), Qt::CaseInsensitive)) {
        r.ok = true;
        r.duplicates = 1;
        r.message = QCoreApplication::translate("Qsl", "eQSL: already there");
        return r;
    }
    if (text.contains(QLatin1String("Bad record"), Qt::CaseInsensitive)
        || text.contains(QLatin1String("Error"), Qt::CaseInsensitive)) {
        r.rejected = 1;
        r.message = QCoreApplication::translate("Qsl", "eQSL: %1").arg(text.left(120));
        return r;
    }
    r.retryLater = true;
    r.message = QCoreApplication::translate("Qsl", "eQSL: unexpected answer");
    return r;
}

QslUploadResult parseClubLogResponse(int status, const QByteArray& body, int qsoCount)
{
    // Club Log risponde in chiaro, una riga: quello che conta e' il codice HTTP,
    // il testo serve a dire all'operatore che cosa ha sbagliato.
    QString text = QString::fromUtf8(body).simplified();
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text = text.simplified().left(160);

    QslUploadResult r;
    const int count = qMax(1, qsoCount);
    if (status == 200) {
        if (text.contains(QLatin1String("dupl"), Qt::CaseInsensitive)) {
            r.ok = true;
            r.duplicates = count;
            r.message = QCoreApplication::translate("Qsl", "Club Log: already there");
            return r;
        }
        if (text.contains(QLatin1String("error"), Qt::CaseInsensitive)
            || text.contains(QLatin1String("invalid"), Qt::CaseInsensitive)) {
            r.rejected = count;
            r.message = QCoreApplication::translate("Qsl", "Club Log: %1").arg(text);
            return r;
        }
        r.ok = true;
        r.accepted = count;
        r.message = text.isEmpty() ? QCoreApplication::translate("Qsl", "Club Log: accepted") : text;
        return r;
    }
    if (status == 400 || status == 401 || status == 403) {
        // Chiave sbagliata, password sbagliata, nominativo non autorizzato: sono
        // cose da sistemare a mano, ritentare non serve.
        r.rejected = count;
        r.message = text.isEmpty()
            ? QCoreApplication::translate("Qsl", "Club Log: refused (%1)").arg(status)
            : QCoreApplication::translate("Qsl", "Club Log: %1").arg(text);
        return r;
    }
    r.retryLater = true;
    r.message = status > 0
        ? QCoreApplication::translate("Qsl", "Club Log: server answered %1").arg(status)
        : QCoreApplication::translate("Qsl", "Club Log: no answer");
    return r;
}

} // namespace qsl

// ── TQSL ──────────────────────────────────────────────────────────────────────

TqslUploader::TqslUploader(QObject* parent)
    : QObject(parent)
    , m_program(qsl::findTqsl())
{
}

void TqslUploader::upload(const QString& adifPath, const QString& location, int qsoCount)
{
    if (m_busy)
        return;
    QslUploadResult error;
    if (m_program.isEmpty() || !QFileInfo::exists(m_program)) {
        error.message = tr("TQSL not found: install Trusted QSL, or set its path in Setup → QSL services");
        emit finished(error);
        return;
    }
    if (!qsl::tqslHasCertificate()) {
        error.message = tr("TQSL has no certificate: import your LoTW certificate in TQSL first");
        emit finished(error);
        return;
    }

    // -u carica su LoTW, -d non chiede l'intervallo di date, -a all accetta i
    // duplicati senza fermarsi, -q esce da solo, -x niente finestra alla fine.
    QStringList args{QStringLiteral("-u"), QStringLiteral("-d"),
                     QStringLiteral("-a"), QStringLiteral("all"),
                     QStringLiteral("-q"), QStringLiteral("-x")};
    if (!location.trimmed().isEmpty())
        args << QStringLiteral("-l") << location.trimmed();
    args << QDir::toNativeSeparators(adifPath);

    m_busy = true;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, [this, qsoCount](int code, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        m_busy = false;
        if (status == QProcess::CrashExit) {
            QslUploadResult crashed;
            crashed.message = tr("TQSL stopped unexpectedly");
            emit finished(crashed);
            return;
        }
        emit finished(qsl::resultFromTqslExit(code, output, qsoCount));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_busy)
            return;
        QslUploadResult failed;
        failed.message = tr("Cannot run TQSL: %1").arg(m_process ? m_process->errorString() : QString());
        m_busy = false;
        emit finished(failed);
    });
    m_process->start(m_program, args);
}

void TqslUploader::cancel()
{
    if (m_process)
        m_process->kill();
}

// ── QRZ Logbook, eQSL e Club Log ──────────────────────────────────────────────

WebQslUploader::WebQslUploader(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void WebQslUploader::setEndpoints(const QUrl& qrz, const QUrl& eqsl)
{
    m_qrzUrl = qrz;
    m_eqslUrl = eqsl;
}

void WebQslUploader::uploadQrz(const QString& apiKey, const QString& adifRecord)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("KEY"), apiKey);
    form.addQueryItem(QStringLiteral("ACTION"), QStringLiteral("INSERT"));
    form.addQueryItem(QStringLiteral("ADIF"), adifRecord);
    send(Service::QrzLogbook, m_qrzUrl, form.toString(QUrl::FullyEncoded).toUtf8());
}

void WebQslUploader::uploadEqsl(const QString& user, const QString& password, const QString& adifRecord)
{
    // eQSL vuole utente e password nell'intestazione dell'ADIF.
    const QString document = QStringLiteral("<EQSL_USER:%1>%2<EQSL_PSWD:%3>%4<EOH>\n%5")
                                 .arg(user.size())
                                 .arg(user)
                                 .arg(password.size())
                                 .arg(password)
                                 .arg(adifRecord);
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("ADIFData"), document);
    send(Service::Eqsl, m_eqslUrl, form.toString(QUrl::FullyEncoded).toUtf8());
}

void WebQslUploader::setClubLogEndpoints(const QUrl& realtime, const QUrl& batch)
{
    m_clubLogRealtimeUrl = realtime;
    m_clubLogBatchUrl = batch;
}

void WebQslUploader::uploadClubLog(const ClubLogAuth& auth, const QByteArray& adifDocument, int qsoCount)
{
    if (m_busy)
        return;
    if (!auth.complete()) {
        QslUploadResult missing;
        missing.message = tr("Club Log: email, password, callsign and API key are all needed");
        emit finished(missing);
        return;
    }

    if (qsoCount <= 1) {
        // Un QSO appena fatto: realtime.php lo aggiunge senza rileggere il log.
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("email"), auth.email);
        form.addQueryItem(QStringLiteral("password"), auth.password);
        form.addQueryItem(QStringLiteral("callsign"), auth.callsign);
        form.addQueryItem(QStringLiteral("api"), auth.apiKey);
        form.addQueryItem(QStringLiteral("adif"), QString::fromUtf8(adifDocument));
        m_busy = true;
        QNetworkRequest request(m_clubLogRealtimeUrl);
        network::useHttp11(request);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
        request.setTransferTimeout(30'000);
        watch(m_net->post(request, form.toString(QUrl::FullyEncoded).toUtf8()), Service::ClubLog, 1);
        return;
    }

    // Piu' QSO insieme: un file ADIF, come se lo si caricasse dal sito.
    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    const auto field = [multi](const QString& name, const QString& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value.toUtf8());
        multi->append(part);
    };
    field(QStringLiteral("email"), auth.email);
    field(QStringLiteral("password"), auth.password);
    field(QStringLiteral("callsign"), auth.callsign);
    field(QStringLiteral("api"), auth.apiKey);
    field(QStringLiteral("clear"), QStringLiteral("0"));   // aggiunge, non cancella il log

    QHttpPart file;
    file.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"file\"; filename=\"decolog.adi\""));
    file.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    file.setBody(adifDocument);
    multi->append(file);

    m_busy = true;
    QNetworkRequest request(m_clubLogBatchUrl);
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(120'000);   // un blocco grosso ci mette di piu'
    QNetworkReply* reply = m_net->post(request, multi);
    multi->setParent(reply);
    watch(reply, Service::ClubLog, qsoCount);
}

namespace {
// Tutte le chiamate a CRX hanno la stessa busta: {"req": {type, query, apikey, ...}}.
QByteArray crxRequest(const QString& query, const QString& apiKey, const QJsonObject& extra)
{
    QJsonObject req{{QStringLiteral("type"), QStringLiteral("radio")},
                    {QStringLiteral("query"), query},
                    {QStringLiteral("apikey"), apiKey}};
    for (auto it = extra.begin(); it != extra.end(); ++it)
        req.insert(it.key(), it.value());
    return QJsonDocument(QJsonObject{{QStringLiteral("req"), req}}).toJson(QJsonDocument::Compact);
}

QNetworkRequest crxHttpRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(30'000);
    return request;
}
} // namespace

void WebQslUploader::uploadCrx(const QString& apiKey, const QJsonObject& qsoData)
{
    if (m_busy)
        return;
    m_busy = true;
    const QByteArray body = crxRequest(QStringLiteral("edit_myqso"), apiKey,
                                       QJsonObject{{QStringLiteral("qsoData"), qsoData}});
    watch(m_net->post(crxHttpRequest(m_crxUrl), body), Service::Crx, 1);
}

void WebQslUploader::listCrxLogs(const QString& apiKey)
{
    QNetworkReply* reply = m_net->post(crxHttpRequest(m_crxUrl), crxRequest(QStringLiteral("get_mylogs"), apiKey, {}));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray answer = reply->readAll();
        QString error;
        QVariantList logs;
        if (reply->error() != QNetworkReply::NoError && status == 0)
            error = network::safeErrorString(reply);
        else if (status == 401)
            error = QCoreApplication::translate("Qsl", "the API key was not accepted");
        else
            logs = qsl::parseCrxLogs(answer, &error);
        emit crxLogsListed(logs, error);
    });
}

void WebQslUploader::send(Service service, const QUrl& url, const QByteArray& body)
{
    if (m_busy)
        return;
    m_busy = true;
    QNetworkRequest request(url);
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(30'000);
    watch(m_net->post(request, body), service, 1);
}

void WebQslUploader::watch(QNetworkReply* reply, Service service, int qsoCount)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, service, qsoCount] {
        reply->deleteLater();
        m_busy = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray answer = reply->readAll();
        if (service == Service::Crx) {
            // CRX dice il motivo nel corpo JSON anche con 400 o 401.
            if (reply->error() != QNetworkReply::NoError && status == 0) {
                QslUploadResult failed;
                failed.retryLater = true;
                failed.message = network::safeErrorString(reply);
                emit finished(failed);
                return;
            }
            emit finished(qsl::parseCrxResponse(status, answer));
            return;
        }
        if (service == Service::ClubLog) {
            // Club Log dice il motivo nel corpo anche quando risponde 403, e
            // quel motivo serve all'operatore piu' del codice.
            if (reply->error() != QNetworkReply::NoError && status == 0) {
                QslUploadResult failed;
                failed.retryLater = true;
                failed.message = network::safeErrorString(reply);
                emit finished(failed);
                return;
            }
            emit finished(qsl::parseClubLogResponse(status, answer, qsoCount));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            QslUploadResult failed;
            failed.retryLater = true;
            failed.message = network::safeErrorString(reply);
            emit finished(failed);
            return;
        }
        emit finished(service == Service::QrzLogbook ? qsl::parseQrzResponse(answer)
                                                     : qsl::parseEqslResponse(answer));
    });
}

} // namespace decolog::core
