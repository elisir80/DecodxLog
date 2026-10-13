// Portato da iu8lmc/Morsecodium (src/decode/): correzione lessicale delle parole CW.
#include "core/Lexicon.h"
#include "core/MorseTables.h"
#include <QStringList>
#include <QSet>
#include <vector>
#include <algorithm>

namespace lexicon {
namespace {

// Vocabolario del traffico CW: Q-code e abbreviazioni comuni + parole inglesi
// frequenti nei QSO. Volutamente COMPATTO: ogni voce in piu' e' un possibile
// "attrattore" di false correzioni.
const QStringList &entries()
{
    static const QStringList list = QStringLiteral(
        "QRL QRM QRN QRP QRS QRT QRV QRX QRZ QSB QSK QSL QSO QSY QTH QRG QRO "
        "CQ DE TU TNX TKS THX AGN PSE FB OM YL XYL DX HW CPY CPI ES HR UR RPT "
        "NR SRI CUL GA GE GM GN GL DR MNI VY ABT NW HPE HV GUD FER CONDX WID "
        "RIG ANT PWR WX RST TEST CFM NIL BURO CALL BAND BEAM FREQ INFO LOOP "
        "NAME OP SIG VERT YAGI DIPOLE HAM "
        "THE AND YOU YOUR FROM HAVE THIS THAT WILL WITH GOOD NICE FINE HOPE "
        "SEE SOON AGAIN THANKS HELLO RADIO MORSE POWER WATTS ANTENNA VERTICAL "
        "WEATHER SUNNY TODAY STATION OPERATOR COPY SIGNAL REPORT WORKING CALLING")
        .split(QLatin1Char(' '));
    return list;
}

const QSet<QString> &entrySet()
{
    static const QSet<QString> set(entries().cbegin(), entries().cend());
    return set;
}

// Sigle delle societa' IARU: e' lo scambio delle stazioni HQ nello IARU HF
// Championship ("5NN RSM"), quindi parole LEGITTIME da non correggere mai
// (RSM -> RST fu una falsa correzione su traffico reale, 11/07/2026).
// Solo protette, NON bersagli di correzione: da attrattori farebbero piu'
// danni dei typo che salverebbero.
const QSet<QString> &protectedSet()
{
    static const QSet<QString> set = [] {
        const QStringList l = QStringLiteral(
            "IARU ARRL RAC ARI DARC RSGB REF URE REP UBA VERON USKA OEVSV "
            "PZK CRC SRAL SSA NRRL EDR IRTS MRASZ ZRS HRS SRS BFRA FRR RSM "
            "RAAG TRAC IARC UARL LRAL LRMD ERAU JARL KARL CRAC NZART WIA "
            "LABRE RCA FMRE IRA")
            .split(QLatin1Char(' '));
        return QSet<QString>(l.cbegin(), l.cend());
    }();
    return set;
}

// Costo di sostituzione tra due lettere: ridotto se i loro pattern Morse
// distano UN elemento (un dit/dah invertito, o un elemento in piu'/in meno),
// pieno altrimenti.
double subCost(QChar a, QChar b)
{
    if (a == b) return 0.0;
    const QString pa = morse::encode(a), pb = morse::encode(b);
    if (pa.isEmpty() || pb.isEmpty()) return 1.0;
    const int la = pa.size(), lb = pb.size();
    if (qAbs(la - lb) > 1) return 1.0;
    if (la == lb) {                          // un elemento invertito?
        int diff = 0;
        for (int i = 0; i < la; ++i) diff += (pa.at(i) != pb.at(i)) ? 1 : 0;
        return diff <= 1 ? 0.5 : 1.0;
    }
    const QString &s = la < lb ? pa : pb;    // un elemento in piu'/in meno?
    const QString &l = la < lb ? pb : pa;
    for (int skip = 0; skip < l.size(); ++skip) {
        bool ok = true;
        for (int i = 0, j = 0; i < l.size() && ok; ++i) {
            if (i == skip) continue;
            ok = (s.at(j++) == l.at(i));
        }
        if (ok) return 0.5;
    }
    return 1.0;
}

// Levenshtein pesato parola-vs-parola: inserzione/cancellazione = 1,
// sostituzione = subCost (0.5 per lettere Morse-vicine).
double editCost(const QString &a, const QString &b)
{
    const int n = a.size(), m = b.size();
    std::vector<double> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j)
            cur[j] = std::min({ prev[j] + 1.0,                       // canc.
                                cur[j - 1] + 1.0,                    // ins.
                                prev[j - 1] + subCost(a.at(i - 1), b.at(j - 1)) });
        std::swap(prev, cur);
    }
    return prev[m];
}

} // namespace

bool contains(const QString &word)
{
    return entrySet().contains(word) || protectedSet().contains(word);
}

QString correct(const QString &word)
{
    // dominio: 3..12 lettere; le cifre indicano rapporti/nominativi/frequenze
    // che non vanno MAI "corretti"
    if (word.size() < 3 || word.size() > 12)
        return word;
    for (const QChar c : word)
        if (c.isDigit()) return word;
    if (entrySet().contains(word) || protectedSet().contains(word))
        return word;

    double best = 1e9, second = 1e9;
    QString bestWord;
    for (const QString &e : entries()) {
        if (qAbs(e.size() - word.size()) > 1) continue;   // pruning
        const double c = editCost(word, e);
        if (c < best) { second = best; best = c; bestWord = e; }
        else if (c < second) { second = c; }
    }
    // budget: UN errore; e serve un vincitore NETTO (margine >= 0.5),
    // altrimenti meglio non toccare la parola
    if (best <= 1.0 && second - best >= 0.5)
        return bestWord;
    return word;
}

} // namespace lexicon
