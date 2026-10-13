// Portato da iu8lmc/Morsecodium (src/decode/), decodificatore CW neurale CTC. Attivo solo con DECODXLOG_CTC_ONNX.
#include "core/MorseTables.h"

namespace morse {

namespace {
struct Entry { const char *code; ushort ch; };

// Mappa carattere -> codice (riferimento: morse.py del progetto originale).
const Entry kTable[] = {
    {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'},
    {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
    {".-",    'A'}, {"-...",  'B'}, {"-.-.",  'C'}, {"-..",   'D'}, {".",     'E'},
    {"..-.",  'F'}, {"--.",   'G'}, {"....",  'H'}, {"..",    'I'}, {".---",  'J'},
    {"-.-",   'K'}, {".-..",  'L'}, {"--",    'M'}, {"-.",    'N'}, {"---",   'O'},
    {".--.",  'P'}, {"--.-",  'Q'}, {".-.",   'R'}, {"...",   'S'}, {"-",     'T'},
    {"..-",   'U'}, {"...-",  'V'}, {".--",   'W'}, {"-..-",  'X'}, {"-.--",  'Y'},
    {"--..",  'Z'},
    // Segni di servizio / punteggiatura comune in CW
    {"-..-.", '/'}, {"-.--.", '('}, {"-.--.-", ')'}, {"-...-", '='}, {".-.-.", '+'},
    {"-....-", '-'}, {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {".----.", '\''},
    {"---...", ':'}, {"-.-.-.", ';'}, {".--.-.", '@'}, {"..--.-", '_'},
    // Lettere accentate (come nel progetto Python)
    {".--.-", 0x00C1 /* Á */}, {".-.-",  0x00C4 /* Ä */}, {"..-..", 0x00C9 /* É */},
    {"--.--", 0x00D1 /* Ñ */}, {"---.",  0x00D6 /* Ö */}, {"..--",  0x00DC /* Ü */},
};
} // namespace

const QHash<QString, QChar> &codeToChar()
{
    static const QHash<QString, QChar> map = [] {
        QHash<QString, QChar> m;
        for (const auto &e : kTable)
            m.insert(QString::fromLatin1(e.code), QChar(e.ch));
        return m;
    }();
    return map;
}

const QHash<QChar, QString> &charToCode()
{
    static const QHash<QChar, QString> map = [] {
        QHash<QChar, QString> m;
        for (const auto &e : kTable) {
            const QChar c(e.ch);
            if (!m.contains(c))                 // prima occorrenza vince
                m.insert(c, QString::fromLatin1(e.code));
        }
        return m;
    }();
    return map;
}

QChar decode(const QString &code, QChar fallback)
{
    return codeToChar().value(code, fallback);
}

namespace {
// Priorita' tra piu' candidati fuzzy (indice basso = piu' probabile): lettere
// per frequenza d'uso in inglese/traffico ham, poi le cifre dei rapporti.
int fuzzyRank(QChar c)
{
    static const QString order = QStringLiteral(
        "ETAINOSHRDLUCMWFGYPBVKJXQZ5901234678/=?");
    const int i = order.indexOf(c);
    return i >= 0 ? i : order.size();
}
} // namespace

QChar decodeFuzzy(const QString &code, QChar fallback)
{
    const auto &map = codeToChar();
    const auto exact = map.constFind(code);
    if (exact != map.cend())
        return *exact;
    // pattern troppo corti sono sempre in tabella; troppo lunghi = rumore puro
    if (code.size() < 2 || code.size() > 8)
        return fallback;

    QChar best = fallback;
    int bestRank = -1;
    auto consider = [&](const QString &cand) {
        const auto it = map.constFind(cand);
        if (it == map.cend())
            return;
        if (it->unicode() >= 128)      // mai "correggere" verso lettere accentate
            return;
        const int r = fuzzyRank(*it);
        if (bestRank < 0 || r < bestRank) { bestRank = r; best = *it; }
    };
    for (int i = 0; i < code.size(); ++i) {   // un elemento classificato male
        QString c = code;
        c[i] = (c[i] == QLatin1Char('.')) ? QLatin1Char('-') : QLatin1Char('.');
        consider(c);
    }
    consider(code.mid(1));                    // elemento spurio in testa
    consider(code.left(code.size() - 1));     // elemento spurio in coda
    return best;
}

QString encode(QChar c)
{
    return charToCode().value(c.toUpper());
}

} // namespace morse
