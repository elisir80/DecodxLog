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

    // Split, i due VFO, RIT e XIT. Non tutte le radio (e non tutti i
    // collegamenti) li sanno fare: features() dice cosa, e il resto risponde
    // failed().
    enum Feature { Split = 1, VfoSelect = 2, Rit = 4, Xit = 8 };
    virtual int features() const { return 0; }
    virtual bool split() const { return false; }
    // In split: la frequenza di trasmissione (il VFO B); 0 se non si sa.
    virtual qint64 txFrequencyHz() const { return 0; }
    virtual QString vfo() const { return {}; }   // "VFOA", "VFOB"
    virtual int ritHz() const { return 0; }
    virtual int xitHz() const { return 0; }
    // Split acceso con la trasmissione su `txHz` (0 = dove sta il VFO B).
    virtual void setSplit(bool on, qint64 txHz = 0)
    {
        Q_UNUSED(on);
        Q_UNUSED(txHz);
        emit failed(tr("This radio link does not do split"));
    }
    virtual void setVfo(const QString& vfo)
    {
        Q_UNUSED(vfo);
        emit failed(tr("This radio link cannot choose the VFO"));
    }
    // 0 spegne.
    virtual void setRit(int hz)
    {
        Q_UNUSED(hz);
        emit failed(tr("This radio link does not do RIT"));
    }
    virtual void setXit(int hz)
    {
        Q_UNUSED(hz);
        emit failed(tr("This radio link does not do XIT"));
    }

signals:
    void changed();
    void failed(const QString& message);
    // Il testo e' stato preso dalla radio: e' partito in aria.
    void morseSent(const QString& text);
    // Questa radio (o questo ponte CAT) il CW non lo sa mandare.
    void morseUnsupported();
};

} // namespace decolog::core
