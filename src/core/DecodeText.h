// DecoDXLog — leggere una riga decodificata (FT8, FT4, FT2…): chi chiama chi.
//
// I messaggi dei modi di WSJT seguono pochi schemi:
//   CQ K1ABC FN42            CQ DX K1ABC FN42        CQ POTA K1ABC FN42
//   IU8LMC K1ABC -12         K1ABC IU8LMC R-05       K1ABC IU8LMC RR73
//   <K1ABC> IU8LMC JN70      K1ABC/P IU8LMC 73
// Il mittente e' il primo nominativo dopo il CQ (e dopo l'eventuale
// "DX", "POTA", "NA"…), oppure il secondo nominativo di un messaggio diretto.
// Il resto — testo libero, scambi di gara — non ha un mittente sicuro e resta
// senza: meglio nessun colore che un colore sbagliato.
#pragma once

#include <QString>

namespace decolog::core::decodetext {

struct Parts {
    QString from;       // chi trasmette, o vuoto se non si capisce
    QString to;         // a chi e' diretto, vuoto per un CQ
    QString grid;       // il locatore, se c'e' (4 o 6 caratteri)
    bool cq{false};     // CQ, QRZ, DE… chiamata generale
    QString modifier;   // "DX", "POTA", "NA"… dopo il CQ
};

// Parte il messaggio. I nominativi sono in maiuscolo, senza le <> dei
// nominativi con hash.
Parts parse(const QString& message);

// Somiglia a un nominativo: lettere e cifre con almeno una cifra e almeno una
// lettera, con eventuale /P, /M, /QRP o prefisso/.
bool looksLikeCall(const QString& token);

// Il nominativo senza i prefissi e i suffissi (/P, /M, /QRP, EA8/…): serve a
// riconoscere la stessa stazione scritta in due modi.
QString baseCall(const QString& call);

// Il messaggio nomina questo nominativo (anche come base: "K1ABC" e "K1ABC/P").
bool mentions(const QString& message, const QString& call);

} // namespace decolog::core::decodetext
