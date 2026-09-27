// DecoDXLog — la radio attraverso flrig (W1HKJ).
//
// flrig tiene la radio e la offre in XML-RPC su HTTP (di solito 127.0.0.1:12345):
// rig.get_vfo, rig.set_vfo, rig.get_mode, rig.set_mode, rig.set_ptt. Chi usa
// fldigi ha quasi sempre flrig aperto: cosi' DecoDXLog ci si affianca senza
// togliere la porta a nessuno. Le richieste vanno una alla volta; frequenza e
// modo si rileggono ogni secondo.
#pragma once

#include "core/RigLink.h"

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <functional>

class QNetworkAccessManager;

namespace decolog::core {

namespace flrig {

// Una chiamata XML-RPC con i suoi parametri (stringhe, interi, double).
QByteArray request(const QString& method, const QVariantList& params = {});
// Il valore di una risposta (stringa, intero, double, array di stringhe); un
// <fault> diventa errore in `fault`.
QVariant parseResponse(const QByteArray& body, QString* fault = nullptr);
// Il modo di flrig (USB, CW-R, DATA-U, PKT-U, USB-D, DIGU...) come lo scrive Hamlib.
QString toHamlibMode(const QString& flrigMode);
// Il nome che flrig capisce per un modo Hamlib, scelto fra quelli della radio.
QString fromHamlibMode(const QString& hamlibMode, const QStringList& available);

} // namespace flrig

class FlrigControl : public RigLink {
    Q_OBJECT

public:
    explicit FlrigControl(QObject* parent = nullptr);

    // "127.0.0.1:12345"
    void connectTo(const QString& address);

    void disconnectFromRig() override;
    bool connected() const override { return m_connected; }
    qint64 frequencyHz() const override { return m_hz; }
    QString mode() const override { return m_mode; }
    int speedWpm() const override { return m_wpm; }
    QString status() const override { return m_status; }

    void refresh() override;
    void setFrequency(qint64 hz) override;
    void setMode(const QString& mode) override;
    void setPtt(bool on) override;
    void setSpeedWpm(int wpm) override;
    void sendMorse(const QString& text) override;
    void stopMorse() override;

private:
    struct Call {
        QString method;
        QVariantList params;
        std::function<void(const QVariant& value, const QString& fault)> done;
    };
    void call(const QString& method, const QVariantList& params,
              std::function<void(const QVariant&, const QString&)> done = {});
    void pump();

    QNetworkAccessManager* m_net;
    QUrl m_url;
    QList<Call> m_queue;
    bool m_busy{false};
    bool m_active{false};
    bool m_connected{false};
    qint64 m_hz{0};
    QString m_mode;
    int m_wpm{24};
    QString m_status;
    QStringList m_modes;
    QTimer m_poll;
    int m_failures{0};
};

} // namespace decolog::core
