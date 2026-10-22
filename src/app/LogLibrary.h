// DecoDXLog — i log della stazione: quello di tutti i giorni e quelli dei contest.
//
// Un contest vuole un log suo: i duplicati, il punteggio e il Cabrillo si
// contano su quel log e su nient'altro, e a fine gara non si vuole che i QSO
// della gara si mescolino con quelli di sempre. Qui c'e' l'elenco dei log
// conosciuti, che sta nelle impostazioni: ognuno e' un file SQLite, e il file
// resta dove l'operatore lo ha messo.
//
// Cambiare log vuol dire riaprire il programma sull'altro file. Sembra brusco,
// ma e' l'unica cosa che non lascia in giro mezzo programma legato al log di
// prima: la radio, il cluster, la sessione di contest e le finestre staccate
// ripartono tutte insieme, come quando si apre il programma.
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QUrl>
#include <QVariantMap>

namespace decolog::app {

class LogLibrary : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList logs READ logs NOTIFY changed)
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(bool askAtStart READ askAtStart WRITE setAskAtStart NOTIFY changed)

public:
    explicit LogLibrary(QObject* parent = nullptr);

    // L'elenco, dal piu' usato di recente: {name, path, current, exists, qsos,
    // lastUsed, missing}.
    QVariantList logs() const;
    QString current() const { return m_current; }
    // I file di tutti i log dell'elenco, anche quelli chiusi.
    QStringList paths() const;
    bool askAtStart() const;
    void setAskAtStart(bool ask);

    // Il log aperto adesso: si segna nell'elenco (e ci entra se non c'era).
    void setCurrent(const QString& path);

    // Crea un log nuovo e lo apre. `name` e' il nome nell'elenco; il file si
    // chiama come il nome, nella cartella dei dati, se `path` e' vuoto.
    // `kind` dice per cosa e' il log: "general" (quello di tutti i giorni),
    // "contest", "award" o "other"; `event` e' la manifestazione o il diploma
    // ("CQ WW SSB", "VUCC"). Senza nome, ma con l'evento, il nome e' l'evento e
    // l'anno. Torna un messaggio d'errore, o "" se e' andata.
    Q_INVOKABLE QString createLog(const QString& name, const QUrl& path = {},
                                  const QString& kind = {}, const QString& event = {});
    // Cancella un log: il file va nel Cestino (si recupera) e il log esce
    // dall'elenco. Il log aperto non si cancella, e se il Cestino non lo prende
    // il file resta dov'e': non si passa mai alla cancellazione definitiva.
    // Torna un messaggio d'errore, o "" se e' andata.
    Q_INVOKABLE QString deleteLog(const QString& path);
    // Mette nell'elenco un log che c'e' gia' sul disco.
    Q_INVOKABLE QString addExisting(const QUrl& path);
    // Riapre il programma su quel log. Non torna se e' andata.
    Q_INVOKABLE QString openLog(const QString& path);
    // Toglie dall'elenco senza cancellare niente dal disco.
    Q_INVOKABLE void forget(const QString& path);
    Q_INVOKABLE void rename(const QString& path, const QString& name);
    // La cartella dove finiscono i log nuovi quando non si sceglie.
    Q_INVOKABLE QString defaultFolder() const;

signals:
    void changed();

private:
    void load();
    void save();

    struct Entry {
        QString name;
        QString path;
        QString lastUsed;   // ISO, per l'ordine
        QString kind;       // general | contest | award | other
        QString event;      // la manifestazione o il diploma, se c'e'
    };

    QList<Entry> m_entries;
    QString m_current;
};

} // namespace decolog::app
