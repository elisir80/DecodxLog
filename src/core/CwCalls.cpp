#include "core/CwCalls.h"

#include "core/DecodeText.h"

#include <QStringList>

#include <algorithm>

namespace decolog::core::cwcalls {

namespace {

bool isSpace(QChar c)
{
    return c == QLatin1Char(' ') || c == QLatin1Char('\n') || c == QLatin1Char('\t');
}

// Le parole del CW che somigliano a un nominativo ma non lo sono, e che il contesto del
// controllo di forma non scarta: sigle di contest e di scambio.
bool excluded(const QString& t)
{
    static const QStringList words{
        QStringLiteral("5NN"), QStringLiteral("599"), QStringLiteral("73"), QStringLiteral("88"),
        QStringLiteral("QRP"), QStringLiteral("QRO"),
    };
    return words.contains(t);
}

struct Token {
    QString text;
    qsizetype from{0};
    qsizetype to{0};   // un oltre la fine
};

QVector<Token> tokenize(const QString& text)
{
    QVector<Token> out;
    qsizetype i = 0;
    const qsizetype n = text.size();
    while (i < n) {
        while (i < n && isSpace(text.at(i)))
            ++i;
        const qsizetype from = i;
        while (i < n && !isSpace(text.at(i)))
            ++i;
        if (i > from)
            out.append({text.mid(from, i - from).toUpper(), from, i});
    }
    return out;
}

} // namespace

bool isCall(const QString& token)
{
    const QString t = token.toUpper();
    if (excluded(t) || !decodetext::looksLikeCall(t))
        return false;
    // Prima della prima cifra ci vuole una lettera, oppure — "9A1AA", "4X6TT" — una cifra e poi
    // una lettera: "40M", "10M", "73S", "100W" non sono nominativi.
    const QStringList parts = t.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part.size() < 3)
            continue;
        qsizetype firstDigit = -1;
        for (qsizetype i = 0; i < part.size(); ++i) {
            if (part.at(i).isDigit()) {
                firstDigit = i;
                break;
            }
        }
        if (firstDigit < 0)
            continue;
        const bool letterFirst = firstDigit >= 1 && std::any_of(part.begin(), part.begin() + firstDigit,
                                                               [](QChar c) { return c.isLetter(); });
        const bool digitLetter = part.size() >= 3 && part.at(0).isDigit() && part.at(1).isLetter()
                                 && part.at(2).isDigit();
        if (letterFirst || digitLetter)
            return true;
    }
    return false;
}

Found find(const QString& text, bool complete)
{
    QVector<Token> tokens = tokenize(text);
    if (tokens.isEmpty())
        return {};
    // L'ultima parola e' a meta' se dopo non c'e' ne' uno spazio ne' la fine della trasmissione.
    if (!complete && !isSpace(text.at(text.size() - 1)))
        tokens.removeLast();

    struct Candidate {
        QString call;
        int score{0};
        int seen{0};
        qsizetype last{0};
        bool cq{false};
    };
    QVector<Candidate> found;
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const QString& t = tokens.at(i).text;
        if (!isCall(t))
            continue;
        int score = 0;
        bool cq = false;
        if (i >= 1 && tokens.at(i - 1).text == QLatin1String("DE"))
            score += 2;
        for (qsizetype k = std::max<qsizetype>(0, i - 3); k < i; ++k) {
            if (tokens.at(k).text == QLatin1String("CQ")) {
                score += 2;
                cq = true;
                break;
            }
        }
        // Il nominativo di chi trasmette sta dopo «DE»; quello della stazione chiamata sta prima.
        // «IU8LMC DE K1ABC»: IU8LMC non prende punti, K1ABC si'.
        auto it = std::find_if(found.begin(), found.end(), [&](const Candidate& c) { return c.call == t; });
        if (it == found.end()) {
            found.append({t, score, 1, i, cq});
        } else {
            it->score = std::max(it->score, score);
            it->seen += 1;
            it->last = i;
            it->cq = it->cq || cq;
        }
    }
    Candidate best;
    for (const Candidate& c : found) {
        const int total = c.score + (c.seen >= 2 ? 1 : 0);
        // Con contesto ci vuole un punto pieno; senza, una ripetizione da sola (K1ABC K1ABC) e'
        // un nominativo detto due volte, che e' quello che si fa per farsi capire.
        const bool valid = c.score >= 2 || c.seen >= 2;
        if (!valid)
            continue;
        if (total > best.score || (total == best.score && c.last > best.last)) {
            best = c;
            best.score = total;
        }
    }
    Found out;
    out.call = best.call;
    out.score = best.score;
    out.cq = best.cq;
    return out;
}

QString toHtml(const QString& text, const QString& linkColor)
{
    QString out;
    out.reserve(text.size() * 2);
    qsizetype i = 0;
    const qsizetype n = text.size();
    while (i < n) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\n')) {
            out += QStringLiteral("<br>");
            ++i;
            continue;
        }
        if (isSpace(c)) {
            out += QLatin1Char(' ');
            ++i;
            continue;
        }
        const qsizetype from = i;
        while (i < n && !isSpace(text.at(i)))
            ++i;
        const QString word = text.mid(from, i - from);
        const QString escaped = word.toHtmlEscaped();
        if (isCall(word)) {
            out += QStringLiteral("<a href=\"call:%1\" style=\"text-decoration:none\">"
                                  "<span style=\"color:%2;font-weight:600\">%3</span></a>")
                       .arg(word.toUpper().toHtmlEscaped(), linkColor, escaped);
        } else {
            out += escaped;
        }
    }
    return out;
}

} // namespace decolog::core::cwcalls
