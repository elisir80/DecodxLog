// DecoDXLog — rimettere a posto un backup del log.
//
// Le copie notturne (VACUUM INTO) servono solo se si possono rimettere: qui si
// guarda cosa c'e' dentro una copia senza toccarla, e la si rimette al posto
// del log.
//
// Il ripristino non butta mai via niente. Prima il log di adesso si copia nella
// cartella dei backup come decodxlog-before-restore-<ora>.sqlite, poi la copia
// scelta prende il suo posto. E si fa con il programma chiuso: gira all'avvio,
// prima di aprire il database, quando il processo di prima e' uscito. Un file
// SQLite sostituito mentre qualcuno lo tiene aperto, con il suo -wal ancora in
// giro, e' il modo piu' sicuro di rovinare un log.
#pragma once

#include <QDateTime>
#include <QFileInfoList>
#include <QString>

namespace decolog::core::logbackup {

// Cosa c'e' in un file di log, letto in sola lettura.
struct Snapshot {
    QString path;
    qint64 bytes{0};
    bool readable{false};  // si apre, e' un log di DecoDXLog, e SQLite lo trova sano
    QString problem;       // perche' no, gia' tradotto
    int qsos{0};
    QDateTime firstQso;
    QDateTime lastQso;
    int schema{0};
};

Snapshot inspect(const QString& path);

// Le copie che DecoDXLog tiene in quella cartella, dalla piu' recente: quelle
// notturne (decolog-*.sqlite) e quelle fatte prima di un ripristino.
QFileInfoList backupsIn(const QString& dir);

// Il nome della copia di sicurezza di un ripristino: non comincia con
// "decolog-", cosi' la pulizia delle copie notturne non la tocca.
bool isSafetyCopy(const QString& fileName);

struct RestoreResult {
    bool ok{false};
    QString error;       // gia' tradotto
    QString safetyCopy;  // dove sta il log com'era prima
    int qsos{0};
};

// Rimette `backup` al posto di `target`. Da chiamare con il log chiuso.
RestoreResult restore(const QString& backup, const QString& target, const QString& safetyDir);

// Aspetta che il processo `pid` sia uscito. false se dopo `timeoutMs` c'e' ancora.
bool waitForProcessExit(qint64 pid, int timeoutMs);

// Quello che fa l'avvio con --restore-from: aspetta che il DecoDXLog di prima
// sia uscito, poi rimette la copia.
RestoreResult restoreAfterExit(qint64 pid, const QString& backup, const QString& target, const QString& safetyDir);

} // namespace decolog::core::logbackup
