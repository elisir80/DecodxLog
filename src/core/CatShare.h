// DecoDXLog — la CAT condivisa, come in Decodium 4.
//
// Quando la radio la tiene DecoDXLog (seriale con il suo rigctld, TCI, flrig,
// OmniRig), nessun altro programma puo' aprirla. Allora DecoDXLog la rivende
// su TCP con il protocollo di rigctld: gli altri programmi — Decodium, WSJT-X,
// un altro log — si collegano come "Hamlib NET rigctl" su 127.0.0.1:4533.
//
// Il dialogo e' quello di Decodium 4 (doc/cat-condivisa-protocollo.md di
// Decodium), verificato con il vero client Hamlib: a \chk_vfo si risponde "0"
// e non "CHKVFO 0", e \dump_state annuncia il modello 1. Un carattere di troppo
// e non si collega nessuno.
//
// Lettura sempre, scrittura su richiesta: di serie i programmi collegati
// leggono soltanto; cambiare frequenza e modo vuole "Consenti il controllo",
// trasmettere vuole anche "Consenti la trasmissione". Solo questo computer.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

class QTcpServer;
class QTcpSocket;

namespace decolog::core {

class RigLink;

class CatShare : public QObject {
    Q_OBJECT

public:
    explicit CatShare(QObject* parent = nullptr);
    ~CatShare() override;

    // La radio da condividere: quella che DecoDXLog sta usando adesso.
    void setRigProvider(std::function<RigLink*()> provider) { m_rig = std::move(provider); }

    // Apre o chiude la porta. false se l'ascolto era chiesto ma non e' riuscito.
    bool configure(bool enabled, int port, bool allowControl, bool allowPtt);

    bool enabled() const { return m_enabled; }
    bool allowControl() const { return m_allowControl; }
    bool allowPtt() const { return m_allowPtt; }
    int port() const { return m_port; }
    bool listening() const;
    int clientCount() const { return static_cast<int>(m_buffers.size()); }
    QString lastError() const { return m_lastError; }

    // Una riga del protocollo e la risposta; vuota (null) = chiudere. Per i test.
    QString handleLine(const QString& line);
    static QString dumpState();

signals:
    void stateChanged();
    // Un programma collegato ha chiesto di cambiare qualcosa: accettato o no.
    void controlAttempt(const QString& command, bool accepted);

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket* s);
    bool refuseWrite(const QString& command, bool pttCommand);

    std::function<RigLink*()> m_rig;
    QTcpServer* m_server{nullptr};
    QHash<QTcpSocket*, QByteArray> m_buffers;
    bool m_enabled{false};
    bool m_allowControl{false};
    bool m_allowPtt{false};
    bool m_pttOn{false};
    int m_port{4533};
    QString m_lastError;
};

} // namespace decolog::core
