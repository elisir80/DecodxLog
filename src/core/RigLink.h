// DecoDXLog — la radio, comunque ci si arrivi.
//
// Alla radio si arriva in due modi: attraverso rigctld, il demone di Hamlib
// (RigControl), o con TCI, il protocollo delle SDR Expert Electronics e di chi
// lo parla (TciControl). Per chi sta sopra — il pannello CW, la barra con la
// frequenza, il cluster che sintonizza — e' la stessa radio: questa e' la
// faccia che hanno in comune.
#pragma once

#include <QObject>
#include <QString>

namespace decolog::core {

class RigLink : public QObject {
    Q_OBJECT

public:
    explicit RigLink(QObject* parent = nullptr) : QObject(parent) {}
    ~RigLink() override = default;

    virtual void disconnectFromRig() = 0;
    virtual bool connected() const = 0;
    virtual qint64 frequencyHz() const = 0;
    // Il modo come lo scrive Hamlib: USB, LSB, CW, AM, FM, PKTUSB, PKTLSB…
    virtual QString mode() const = 0;
    virtual int speedWpm() const = 0;
    // Cosa sta succedendo, in una riga da mostrare a chi guarda.
    virtual QString status() const = 0;

    virtual void refresh() = 0;
    virtual void setFrequency(qint64 hz) = 0;
    // Il modo come lo scrive Hamlib (USB, CW, PKTUSB…).
    virtual void setMode(const QString& mode) = 0;
    virtual void setPtt(bool on) = 0;
    virtual void setSpeedWpm(int wpm) = 0;
    virtual void sendMorse(const QString& text) = 0;
    virtual void stopMorse() = 0;

signals:
    void changed();
    void failed(const QString& message);
    // Il testo e' stato preso dalla radio: e' partito in aria.
    void morseSent(const QString& text);
    // Questa radio (o questo ponte CAT) il CW non lo sa mandare.
    void morseUnsupported();
};

} // namespace decolog::core
