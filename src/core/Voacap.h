// DecoDXLog — la previsione di propagazione con VOACAP.
//
// VOACAP e' il motore dell'ITS (Institute for Telecommunication Sciences):
// mappe CCIR dello strato F2, strati E ed Es, assorbimento, rumore, guadagno
// delle antenne, e per ogni ora e frequenza la probabilita' che il segnale
// arrivi con il rapporto segnale/rumore che serve (REL). DecoDXLog porta con se'
// voacapl (third_party/voacapl), lo lancia su un percorso e legge l'uscita.
//
// Qui ci sono i pezzi che non dipendono dal programma — la scheda d'ingresso
// scritta colonna per colonna come VOACAP la legge (formati Fortran fissi), il
// file d'antenna, la lettura dell'uscita — e il motore che lo fa girare su un
// altro processo, uno per volta.
#pragma once

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <optional>

class QProcess;
class QTimer;

namespace decolog::core::voacap {

struct Request {
    double fromLat{0.0}, fromLon{0.0};
    double toLat{0.0}, toLon{0.0};
    QString fromLabel, toLabel;
    int year{2026};
    int month{1};
    double ssn{100.0};            // numero di macchie solari
    double powerWatts{100.0};
    double txGainDbi{2.0};        // antenne isotrope con quel guadagno
    double rxGainDbi{2.0};
    int noise{145};               // rumore artificiale a 3 MHz, in -dBW
    double requiredSnr{16.0};     // dB-Hz, vedi requiredSnrFor()
    double minAngle{3.0};         // gradi sopra l'orizzonte
    QList<double> mhz;            // al massimo 11, fra 2 e 30 MHz
    bool longPath{false};

    // Due richieste con la stessa chiave danno lo stesso risultato.
    QString key() const;
};

struct Cell {
    double rel{0.0};              // 0..1: la probabilita' di farcela
    double snr{0.0};              // dB, mediano
    double signalDbw{0.0};        // S DBW
    double mufDays{0.0};          // la frazione dei giorni con la frequenza sotto la MUF
    QString mode;                 // "1F2", "2 E", ...
};

struct HourResult {
    int hourUtc{0};
    double muf{0.0};
    QList<Cell> cells;            // una per frequenza richiesta, nello stesso ordine
};

struct Result {
    bool valid{false};
    QString error;
    QList<double> mhz;
    QList<HourResult> hours;      // 24, dalle 00 UTC
};

// Il rapporto segnale/rumore che serve (dB-Hz) per un modo: la soglia di
// decodifica o di ascolto nella sua banda, piu' 10·log10 della banda, piu' un
// margine. FT8 -21 dB in 2,5 kHz, FT4 -17,5, CW -3 dB in 500 Hz (un buon
// orecchio), RTTY +8 dB in 250 Hz, SSB +6 dB in 2,4 kHz.
double requiredSnrFor(const QString& mode);
// Da 0 (chiuso) a 3 (buono), con la stessa scala del modello semplice.
int qualityOf(const Cell& cell);

// La scheda d'ingresso per un percorso, 24 ore, METHOD 30 (percorso corto e
// lungo mescolati come fa VOACAP), antenne isotrope con il loro guadagno.
QString deck(const Request& request);
// L'uscita di METHOD 30: MUF, e per ogni frequenza REL, SNR, S DBW, modo.
Result parse(const QString& output);

// Il motore: voacapl.exe con i suoi dati, lanciato su un altro processo.
class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(QObject* parent = nullptr);
    ~Engine() override;

    // Dove sta il programma: la cartella "voacap" accanto a DecoDXLog.exe, o
    // quella di DECODXLOG_VOACAP_DIR (le prove, una compilazione).
    static QString bundledDir();
    bool available() const;
    // La cartella scrivibile dove VOACAP lavora (i suoi dati vi si copiano).
    void setWorkDir(const QString& dir);
    QString workDir() const { return m_work; }
    bool busy() const { return m_process != nullptr; }

    // Uno per volta: quello chiesto mentre un altro gira aspetta, e uno
    // chiesto dopo ancora prende il suo posto.
    void run(const Request& request);

signals:
    void finished(const QString& key, const decolog::core::voacap::Result& result);

private:
    bool prepare(QString* error);
    void start(const Request& request);
    void done(const Result& result);

    QString m_bundled;
    QString m_work;
    bool m_prepared{false};
    QProcess* m_process{nullptr};
    QTimer* m_timeout{nullptr};
    Request m_current;
    std::optional<Request> m_next;
};

} // namespace decolog::core::voacap

Q_DECLARE_METATYPE(decolog::core::voacap::Result)
