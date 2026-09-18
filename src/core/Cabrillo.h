// DecoLog — il log di un contest come lo vuole chi lo riceve.
//
// Cabrillo 3.0: un file di testo con una testata di righe "PAROLA: valore" e una
// riga per QSO, a colonne fisse. Le fanno tutti uguali perche' i robot che
// ricevono i log leggono le colonne, non gli spazi: un campo fuori posto e il log
// torna indietro.
//
// La riga e':
//   QSO: <freq> <mo> <data> <ora> <mio call> <mio rst> <mio scambio> <suo call> ...
#pragma once

#include "core/Adif.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace decolog::core::cabrillo {

// La testata. I valori predefiniti sono quelli del caso piu' comune: singolo
// operatore, bassa potenza, tutte le bande.
struct Info {
    QString contest;            // CQ-WW-CW, ARRL-DX-SSB, IARU-HF...
    QString callsign;
    QString categoryOperator{QStringLiteral("SINGLE-OP")};
    QString categoryAssisted{QStringLiteral("NON-ASSISTED")};
    QString categoryBand{QStringLiteral("ALL")};
    QString categoryPower{QStringLiteral("LOW")};
    QString categoryMode{QStringLiteral("MIXED")};
    QString categoryTransmitter{QStringLiteral("ONE")};
    QString categoryOverlay;
    QString gridLocator;
    QString location;           // la sezione o la zona, secondo il contest
    QString club;
    QString name;
    QStringList address;
    QString email;
    QString operators;
    qint64  claimedScore{0};
    QStringList soapbox;
};

// Il modo come lo scrive Cabrillo: CW, PH, RY, DG, FM.
QString modeCode(const QString& mode, const QString& submode);
// La frequenza: kHz per HF, il numero della banda per VHF e oltre.
QString frequencyField(const QString& freqMhz, const QString& band);
// Una riga QSO: torna vuota se al QSO manca qualcosa di indispensabile.
QString qsoLine(const Info& info, const AdifRecord& record);

// Il file intero. `error` dice cosa manca quando torna vuoto.
QByteArray write(const Info& info, const QList<AdifRecord>& qsos, QString* error = nullptr);

} // namespace decolog::core::cabrillo
