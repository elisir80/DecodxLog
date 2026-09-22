#include "core/ContestRules.h"

#include "core/Awards.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

namespace decolog::core {
namespace contestrules {

namespace {

// lupdate non segue una funzione d'aiuto: cosi' vede il contesto e le frasi
// finiscono dove il programma le cerca.
struct Tr {
    Q_DECLARE_TR_FUNCTIONS(ContestRules)
};

// Le bande basse del CQ WPX, dove un QSO vale il doppio.
bool lowBand(const QString& band)
{
    const QString b = band.toLower();
    return b == QLatin1String("40m") || b == QLatin1String("80m") || b == QLatin1String("160m");
}

bool sameCountry(const ContestQso& qso, const ContestStation& me)
{
    return qso.dxcc > 0 && me.dxcc > 0 && qso.dxcc == me.dxcc;
}

bool sameContinent(const ContestQso& qso, const ContestStation& me)
{
    return !qso.continent.isEmpty() && !me.continent.isEmpty()
           && qso.continent.compare(me.continent, Qt::CaseInsensitive) == 0;
}

bool bothInNorthAmerica(const ContestQso& qso, const ContestStation& me)
{
    return qso.continent.compare(QLatin1String("NA"), Qt::CaseInsensitive) == 0
           && me.continent.compare(QLatin1String("NA"), Qt::CaseInsensitive) == 0;
}

// Italia (248) e Sardegna (225): due entita' DXCC, un paese solo.
bool italianEntity(int dxcc)
{
    return dxcc == 248 || dxcc == 225;
}

QString modeGroup(const QString& mode)
{
    const QString m = mode.toUpper();
    if (m == QLatin1String("CW"))
        return QStringLiteral("CW");
    if (m == QLatin1String("SSB") || m == QLatin1String("AM") || m == QLatin1String("FM"))
        return QStringLiteral("SSB");
    return QStringLiteral("RTTY");   // per i contest ARI il resto e' digitale
}

} // namespace

ContestRules forId(const QString& contestId)
{
    const QString id = contestId.trimmed().toUpper();
    ContestRules r;
    r.id = id;
    r.valid = true;

    if (id.startsWith(QLatin1String("CQ-WW-"))) {
        r.exchange = ContestRules::Exchange::CqZone;
        r.exchangeLabel = Tr::tr("CQ zone");
        r.source = QStringLiteral("cqww.com/rules");
        return r;
    }
    if (id.startsWith(QLatin1String("CQ-WPX-"))) {
        r.exchange = ContestRules::Exchange::Serial;
        r.exchangeLabel = Tr::tr("Number");
        r.source = QStringLiteral("cqwpx.com/rules");
        return r;
    }
    if (id == QLatin1String("IARU-HF")) {
        r.exchange = ContestRules::Exchange::ItuZone;
        r.exchangeLabel = Tr::tr("ITU zone or HQ");
        r.source = QStringLiteral("contests.arrl.org — IARU HF Rules 1.21");
        return r;
    }
    if (id == QLatin1String("ARI-DX")) {
        // Le italiane mandano la provincia, le altre il progressivo: il campo e'
        // uno solo e accetta tutte e due.
        r.exchange = ContestRules::Exchange::Province;
        r.exchangeLabel = Tr::tr("Province or number");
        r.source = QStringLiteral("ARI International DX Contest");
        return r;
    }
    if (id == QLatin1String("ARI-SEZIONI")) {
        r.exchange = ContestRules::Exchange::AriSection;
        r.exchangeLabel = Tr::tr("ARI section (ASC)");
        r.source = QStringLiteral("ARI — Contest delle Sezioni 2026");
        return r;
    }
    if (id == QLatin1String("ARI-40-80")) {
        r.exchange = ContestRules::Exchange::Province;
        r.exchangeLabel = Tr::tr("Province");
        r.source = QStringLiteral("ARI — Contest 40/80");
        return r;
    }

    r.valid = false;
    return r;
}

QStringList known()
{
    return {QStringLiteral("CQ-WW-SSB"),  QStringLiteral("CQ-WW-CW"),   QStringLiteral("CQ-WW-RTTY"),
            QStringLiteral("CQ-WPX-SSB"), QStringLiteral("CQ-WPX-CW"),  QStringLiteral("CQ-WPX-RTTY"),
            QStringLiteral("IARU-HF"),    QStringLiteral("ARI-DX"),
            QStringLiteral("ARI-SEZIONI"), QStringLiteral("ARI-40-80")};
}

int points(const ContestRules& rules, const ContestQso& qso, const ContestStation& me)
{
    if (!rules.valid)
        return 0;
    const QString id = rules.id;

    // CQ WW (regolamento VII): stesso paese 0, stesso continente 1 (2 fra
    // stazioni del Nord America), continente diverso 3.
    if (id.startsWith(QLatin1String("CQ-WW-"))) {
        if (sameCountry(qso, me))
            return 0;
        if (sameContinent(qso, me))
            return bothInNorthAmerica(qso, me) ? 2 : 1;
        return 3;
    }

    // CQ WPX (regolamento VI): sulle bande alte 3 punti fra continenti diversi,
    // 1 dentro il continente; sulle basse il doppio. Il proprio paese vale 1
    // punto su qualunque banda. Fra stazioni del Nord America il doppio del
    // punteggio "stesso continente".
    if (id.startsWith(QLatin1String("CQ-WPX-"))) {
        if (sameCountry(qso, me))
            return 1;
        const bool low = lowBand(qso.band);
        if (sameContinent(qso, me)) {
            const int base = bothInNorthAmerica(qso, me) ? 2 : 1;
            return low ? base * 2 : base;
        }
        return low ? 6 : 3;
    }

    // IARU HF (regolamento 5.1): stessa zona ITU 1 punto, stazione HQ 1 punto,
    // stessa zona ma altro continente 1, stesso continente altra zona 3,
    // continente e zona diversi 5.
    if (id == QLatin1String("IARU-HF")) {
        static const QRegularExpression onlyDigits(QStringLiteral("^\\d+$"));
        const bool headquarters = !qso.exchange.isEmpty()
                                  && !onlyDigits.match(qso.exchange.trimmed()).hasMatch();
        if (headquarters)
            return 1;
        const int zone = qso.ituZone > 0 ? qso.ituZone : qso.exchange.toInt();
        if (zone > 0 && me.ituZone > 0 && zone == me.ituZone)
            return 1;
        if (sameContinent(qso, me))
            return 3;
        return 5;
    }

    // ARI DX: 10 punti con una stazione italiana, 3 con un altro continente,
    // 1 nel proprio continente, 0 nel proprio paese (che vale come
    // moltiplicatore).
    if (id == QLatin1String("ARI-DX")) {
        if (italianEntity(qso.dxcc))
            return 10;
        if (sameCountry(qso, me))
            return 0;
        return sameContinent(qso, me) ? 1 : 3;
    }

    // Contest delle Sezioni ARI (punto 7): i punti stanno nella banda.
    if (id == QLatin1String("ARI-SEZIONI")) {
        const QString b = qso.band.toLower();
        if (b == QLatin1String("40m"))
            return 1;
        if (b == QLatin1String("80m") || b == QLatin1String("20m"))
            return 2;
        if (b == QLatin1String("160m") || b == QLatin1String("15m"))
            return 3;
        if (b == QLatin1String("10m"))
            return 4;
        return 0;
    }

    // Contest 40/80: i punti stanno nel modo.
    if (id == QLatin1String("ARI-40-80")) {
        const QString m = modeGroup(qso.mode);
        if (m == QLatin1String("CW"))
            return 3;
        if (m == QLatin1String("RTTY"))
            return 2;
        return 1;
    }

    return 0;
}

QStringList multipliers(const ContestRules& rules, const ContestQso& qso, const ContestStation& me)
{
    if (!rules.valid)
        return {};
    const QString id = rules.id;
    const QString band = qso.band.toLower();

    // CQ WW: la zona e il paese, ognuno una volta per banda.
    if (id.startsWith(QLatin1String("CQ-WW-"))) {
        QStringList out;
        const int zone = qso.cqZone > 0 ? qso.cqZone : qso.exchange.toInt();
        if (zone >= 1 && zone <= 40)
            out << QStringLiteral("zona %1|%2").arg(zone).arg(band);
        if (qso.dxcc > 0)
            out << QStringLiteral("paese %1|%2").arg(qso.dxcc).arg(band);
        return out;
    }

    // CQ WPX: il prefisso, una volta sola su tutte le bande.
    if (id.startsWith(QLatin1String("CQ-WPX-"))) {
        const QString prefix = awards::wpxPrefix(qso.call);
        return prefix.isEmpty() ? QStringList{} : QStringList{QStringLiteral("prefisso %1").arg(prefix)};
    }

    // IARU HF (5.2): zone ITU e stazioni HQ, per banda. Una HQ non porta la zona.
    if (id == QLatin1String("IARU-HF")) {
        static const QRegularExpression onlyDigits(QStringLiteral("^\\d+$"));
        const QString exchange = qso.exchange.trimmed().toUpper();
        if (!exchange.isEmpty() && !onlyDigits.match(exchange).hasMatch())
            return {QStringLiteral("hq %1|%2").arg(exchange, band)};
        const int zone = qso.ituZone > 0 ? qso.ituZone : exchange.toInt();
        if (zone >= 1 && zone <= 90)
            return {QStringLiteral("zona %1|%2").arg(zone).arg(band)};
        return {};
    }

    // ARI DX: le province italiane e i paesi DXCC (Italia e Sardegna escluse
    // come paese), una volta per banda.
    if (id == QLatin1String("ARI-DX")) {
        if (italianEntity(qso.dxcc)) {
            const QString province = awards::italianProvince(qso.exchange);
            return province.isEmpty() ? QStringList{}
                                      : QStringList{QStringLiteral("prov %1|%2").arg(province, band)};
        }
        return qso.dxcc > 0 ? QStringList{QStringLiteral("paese %1|%2").arg(qso.dxcc).arg(band)}
                            : QStringList{};
    }

    // Contest delle Sezioni (punto 8): i codici ASC, fino a tre volte per banda
    // — uno per modo.
    if (id == QLatin1String("ARI-SEZIONI")) {
        static const QRegularExpression asc(QStringLiteral("^([A-Z])\\s*(\\d{2})$"));
        const auto match = asc.match(qso.exchange.trimmed().toUpper());
        if (!match.hasMatch())
            return {};
        return {QStringLiteral("sez %1%2|%3|%4")
                    .arg(match.captured(1), match.captured(2), band, modeGroup(qso.mode))};
    }

    // Contest 40/80: le province, fino a tre volte per banda (una per modo).
    if (id == QLatin1String("ARI-40-80")) {
        const QString province = awards::italianProvince(qso.exchange);
        return province.isEmpty() ? QStringList{}
                                  : QStringList{QStringLiteral("prov %1|%2|%3")
                                                    .arg(province, band, modeGroup(qso.mode))};
    }

    Q_UNUSED(me);
    return {};
}

QString checkExchange(const ContestRules& rules, const QString& exchange)
{
    if (!rules.valid)
        return {};
    const QString text = exchange.trimmed().toUpper();
    if (text.isEmpty())
        return Tr::tr("The exchange is missing.");

    switch (rules.exchange) {
    case ContestRules::Exchange::Serial: {
        static const QRegularExpression digits(QStringLiteral("^\\d{1,6}$"));
        return digits.match(text).hasMatch() ? QString() : Tr::tr("A number was expected.");
    }
    case ContestRules::Exchange::CqZone: {
        bool ok = false;
        const int zone = text.toInt(&ok);
        return ok && zone >= 1 && zone <= 40 ? QString() : Tr::tr("A CQ zone goes from 1 to 40.");
    }
    case ContestRules::Exchange::ItuZone: {
        // Una stazione HQ manda la sigla della societa' invece della zona.
        static const QRegularExpression digits(QStringLiteral("^\\d+$"));
        if (!digits.match(text).hasMatch())
            return text.size() <= 6 ? QString() : Tr::tr("An ITU zone or a society abbreviation.");
        const int zone = text.toInt();
        return zone >= 1 && zone <= 90 ? QString() : Tr::tr("An ITU zone goes from 1 to 90.");
    }
    case ContestRules::Exchange::Province: {
        // Le stazioni non italiane mandano il progressivo: va bene lo stesso.
        static const QRegularExpression digits(QStringLiteral("^\\d{1,6}$"));
        if (digits.match(text).hasMatch())
            return {};
        return awards::italianProvince(text).isEmpty()
                   ? Tr::tr("%1 is not an Italian province.").arg(text) : QString();
    }
    case ContestRules::Exchange::AriSection: {
        static const QRegularExpression asc(QStringLiteral("^[A-Z]\\s*\\d{2}$"));
        return asc.match(text).hasMatch() ? QString()
                                          : Tr::tr("An ASC code is a letter and two digits, like L01.");
    }
    case ContestRules::Exchange::Grid: {
        static const QRegularExpression grid(QStringLiteral("^[A-R]{2}\\d{2}([A-X]{2})?$"));
        return grid.match(text).hasMatch() ? QString() : Tr::tr("A locator was expected, like JN70.");
    }
    case ContestRules::Exchange::None:
        return {};
    }
    return {};
}

} // namespace contestrules
} // namespace decolog::core
