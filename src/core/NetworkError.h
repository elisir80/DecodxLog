// DecoDXLog — messaggi d'errore di rete senza segreti.
//
// QNetworkReply::errorString() riporta l'URL intero ("Error transferring
// https://…?password=… - server replied: …"). QRZ, HamQTH e LoTW vogliono la
// password nella query, quindi l'URL va tolto prima che il messaggio arrivi al
// registro attivita' o allo schermo.
#pragma once

#include <QNetworkReply>
#include <QString>
#include <QUrl>

namespace decolog::core::network {

// Alcuni endpoint pubblici chiudono gli stream HTTP/2 senza completare il
// protocollo. Per le API usate da DecoDXLog HTTP/1.1 e' piu' interoperabile e
// conserva lo stesso TLS, timeout e gestione degli errori.
inline void useHttp11(QNetworkRequest& request)
{
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
}

inline QString safeErrorString(const QNetworkReply* reply)
{
    QString message = reply->errorString();
    const QUrl url = reply->url();
    const QString bare = url.toString(QUrl::RemoveQuery | QUrl::RemoveUserInfo);
    for (const QString& full : {url.toString(), url.toString(QUrl::FullyEncoded), url.toDisplayString(),
                                QString::fromUtf8(url.toEncoded())}) {
        if (!full.isEmpty())
            message.replace(full, bare);
    }
    // Se l'URL compare in una forma che non si e' riconosciuta, la query se ne va
    // comunque: tutto quello che segue un '?' fino al primo spazio.
    const qsizetype mark = message.indexOf(QLatin1Char('?'));
    if (mark >= 0) {
        qsizetype end = message.indexOf(QLatin1Char(' '), mark);
        if (end < 0)
            end = message.size();
        message.remove(mark, end - mark);
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0 && !message.contains(QString::number(status)))
        message += QStringLiteral(" (HTTP %1)").arg(status);
    return message;
}

} // namespace decolog::core::network
