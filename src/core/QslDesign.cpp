#include "core/QslDesign.h"

#include <QColor>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QJsonArray>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QRectF>

namespace decolog::core::qsldesign {

namespace {

// Il foglio su cui si stampa: A4 a 300 punti per pollice, che e' la misura in
// cui una cartolina viene ancora bene.
constexpr int kPdfDpi = 300;

// Il rapporto di una cartolina vera, se il modello non c'e': 148 x 105 mm.
constexpr double kPostcardRatio = 148.0 / 105.0;

QString twoDigits(int value)
{
    return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
}

// La data del QSO come la scrive il log: "2026-03-21".
QDate dateOf(const QVariantMap& qso)
{
    return QDate::fromString(qso.value(QStringLiteral("date")).toString(), Qt::ISODate);
}

} // namespace

QStringList keys()
{
    return {
        QStringLiteral("call"), QStringLiteral("date"), QStringLiteral("day"),
        QStringLiteral("month"), QStringLiteral("monthName"), QStringLiteral("year"),
        QStringLiteral("time"), QStringLiteral("freq"), QStringLiteral("band"),
        QStringLiteral("mode"), QStringLiteral("rst"), QStringLiteral("name"),
        QStringLiteral("qth"), QStringLiteral("country"), QStringLiteral("grid"),
        QStringLiteral("via"), QStringLiteral("myCall"), QStringLiteral("myGrid"),
        QStringLiteral("myName"), QStringLiteral("myQth"), QStringLiteral("text"),
    };
}

QString valueFor(const Field& field, const QVariantMap& qso, const QVariantMap& station)
{
    const QString key = field.key;
    if (key == QLatin1String("text"))
        return field.text;
    if (key == QLatin1String("day")) {
        const QDate date = dateOf(qso);
        return date.isValid() ? twoDigits(date.day()) : QString();
    }
    if (key == QLatin1String("month")) {
        const QDate date = dateOf(qso);
        return date.isValid() ? twoDigits(date.month()) : QString();
    }
    if (key == QLatin1String("monthName")) {
        const QDate date = dateOf(qso);
        // In inglese e in maiuscolo: su una QSL lo leggono tutti, in qualunque
        // paese arrivi.
        static const char* names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        return date.isValid() ? QString::fromLatin1(names[date.month() - 1]) : QString();
    }
    if (key == QLatin1String("year")) {
        const QDate date = dateOf(qso);
        return date.isValid() ? QString::number(date.year()) : QString();
    }
    if (key == QLatin1String("time")) {
        const QString time = qso.value(QStringLiteral("time")).toString();
        // "1234" diventa "12:34": sulla cartolina si legge meglio.
        return time.size() == 4 ? time.left(2) + QLatin1Char(':') + time.mid(2) : time;
    }
    if (key == QLatin1String("freq")) {
        const double mhz = qso.value(QStringLiteral("freq")).toDouble();
        return mhz > 0 ? QString::number(mhz, 'f', 3) : QString();
    }
    if (key.startsWith(QLatin1String("my"))) {
        QString what = key.mid(2);
        what[0] = what.at(0).toLower();
        return station.value(what).toString();
    }
    return qso.value(key).toString();
}

QJsonObject toJson(const Card& card)
{
    QJsonArray fields;
    for (const Field& f : card.fields) {
        fields << QJsonObject{
            {QStringLiteral("key"), f.key},
            {QStringLiteral("text"), f.text},
            {QStringLiteral("x"), f.x},
            {QStringLiteral("y"), f.y},
            {QStringLiteral("size"), f.size},
            {QStringLiteral("bold"), f.bold},
            {QStringLiteral("color"), f.color},
            {QStringLiteral("align"), f.align},
            {QStringLiteral("family"), f.family},
        };
    }
    return QJsonObject{{QStringLiteral("template"), card.templatePath},
                       {QStringLiteral("fields"), fields}};
}

Card fromJson(const QJsonObject& object)
{
    Card card;
    card.templatePath = object.value(QStringLiteral("template")).toString();
    for (const QJsonValue& value : object.value(QStringLiteral("fields")).toArray()) {
        const QJsonObject o = value.toObject();
        Field f;
        f.key = o.value(QStringLiteral("key")).toString();
        if (f.key.isEmpty())
            continue;
        f.text = o.value(QStringLiteral("text")).toString();
        f.x = qBound(0.0, o.value(QStringLiteral("x")).toDouble(0.5), 1.0);
        f.y = qBound(0.0, o.value(QStringLiteral("y")).toDouble(0.5), 1.0);
        f.size = qBound(5, o.value(QStringLiteral("size")).toInt(40), 400);
        f.bold = o.value(QStringLiteral("bold")).toBool(true);
        const QString color = o.value(QStringLiteral("color")).toString();
        f.color = QColor::isValidColorName(color) ? color : QStringLiteral("#000000");
        const QString align = o.value(QStringLiteral("align")).toString();
        f.align = (align == QLatin1String("center") || align == QLatin1String("right"))
                      ? align : QStringLiteral("left");
        f.family = o.value(QStringLiteral("family")).toString();
        card.fields << f;
    }
    return card;
}

void paint(QPainter& painter, const QRectF& target, const QImage& background,
           const Card& card, const QVariantMap& qso, const QVariantMap& station)
{
    painter.save();
    if (background.isNull()) {
        painter.fillRect(target, Qt::white);
        painter.setPen(QPen(Qt::gray, 1));
        painter.drawRect(target);
    } else {
        painter.drawImage(target, background);
    }

    for (const Field& f : card.fields) {
        const QString text = valueFor(f, qso, station);
        if (text.isEmpty())
            continue;
        QFont font(f.family.isEmpty() ? painter.font().family() : f.family);
        // Il corpo e' in millesimi dell'altezza: la stessa cartolina stampata
        // grande o piccola resta uguale a se stessa.
        const double pixels = qMax(1.0, target.height() * f.size / 1000.0);
        font.setPixelSize(qRound(pixels));
        font.setBold(f.bold);
        painter.setFont(font);
        painter.setPen(QColor(f.color));

        const QFontMetricsF metrics(font);
        const double width = metrics.horizontalAdvance(text);
        double left = target.left() + target.width() * f.x;
        if (f.align == QLatin1String("center"))
            left -= width / 2.0;
        else if (f.align == QLatin1String("right"))
            left -= width;
        // La y e' il bordo di sopra del testo, come in QML: cosi' quello che si
        // vede trascinando e' quello che si stampa.
        const double baseline = target.top() + target.height() * f.y + metrics.ascent();
        painter.drawText(QPointF(left, baseline), text);
    }
    painter.restore();
}

namespace {

// Le caselle in cui dividere un foglio: 1, 2 o 4 cartoline per pagina.
QList<QRectF> cardCells(const QRectF& page, int perPage, double ratio)
{
    const int columns = perPage >= 4 ? 2 : 1;
    const int rows = perPage >= 2 ? 2 : 1;
    const double margin = qMin(page.width(), page.height()) * 0.03;
    const double cellW = (page.width() - margin * (columns + 1)) / columns;
    const double cellH = (page.height() - margin * (rows + 1)) / rows;

    QList<QRectF> out;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            if (out.size() >= perPage)
                break;
            // Dentro la casella la cartolina tiene le sue proporzioni: una QSL
            // stirata non la manda nessuno.
            double w = cellW;
            double h = w / ratio;
            if (h > cellH) {
                h = cellH;
                w = h * ratio;
            }
            const double x = page.left() + margin + c * (cellW + margin) + (cellW - w) / 2.0;
            const double y = page.top() + margin + r * (cellH + margin) + (cellH - h) / 2.0;
            out << QRectF(x, y, w, h);
        }
    }
    return out;
}

} // namespace

bool writePdf(const QString& path, const Card& card, const QList<QVariantMap>& qsos,
              const QVariantMap& station, int perPage, QString* error)
{
    auto fail = [error](const QString& text) {
        if (error)
            *error = text;
        return false;
    };
    if (qsos.isEmpty())
        return fail(QStringLiteral("no QSO"));

    QImage background(card.templatePath);
    const double ratio = background.isNull() || background.height() == 0
                             ? kPostcardRatio
                             : double(background.width()) / background.height();

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(kPdfDpi);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle(QStringLiteral("DecoDXLog QSL"));

    QPainter painter;
    if (!painter.begin(&writer))
        return fail(QStringLiteral("cannot write %1").arg(QDir::toNativeSeparators(path)));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF page(0, 0, writer.width(), writer.height());
    const int per = qBound(1, perPage, 4);
    const QList<QRectF> cells = cardCells(page, per, ratio);

    for (int i = 0; i < qsos.size(); ++i) {
        if (i > 0 && i % cells.size() == 0)
            writer.newPage();
        paint(painter, cells.at(i % cells.size()), background, card, qsos.at(i), station);
    }
    painter.end();
    return true;
}

bool writePng(const QString& dir, const Card& card, const QList<QVariantMap>& qsos,
              const QVariantMap& station, QStringList* written, QString* error)
{
    auto fail = [error](const QString& text) {
        if (error)
            *error = text;
        return false;
    };
    if (qsos.isEmpty())
        return fail(QStringLiteral("no QSO"));
    QDir target(dir);
    if (!target.exists() && !QDir().mkpath(dir))
        return fail(QStringLiteral("cannot make %1").arg(QDir::toNativeSeparators(dir)));

    const QImage background(card.templatePath);
    // Senza modello si fa comunque una cartolina, alla misura di una vera a
    // 300 punti per pollice.
    const QSize size = background.isNull()
                           ? QSize(qRound(148.0 / 25.4 * kPdfDpi), qRound(105.0 / 25.4 * kPdfDpi))
                           : background.size();

    for (const QVariantMap& qso : qsos) {
        QImage image(size, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        paint(painter, QRectF(QPointF(0, 0), QSizeF(size)), background, card, qso, station);
        painter.end();

        const QString call = qso.value(QStringLiteral("call")).toString().toUpper();
        QString safe = call;
        safe.replace(QRegularExpression(QStringLiteral("[^A-Z0-9]")), QStringLiteral("_"));
        const QString name = QStringLiteral("%1-%2-%3.png")
                                 .arg(safe.isEmpty() ? QStringLiteral("QSL") : safe,
                                      qso.value(QStringLiteral("date")).toString().remove(QLatin1Char('-')),
                                      qso.value(QStringLiteral("time")).toString());
        const QString file = target.filePath(name);
        if (!image.save(file, "PNG"))
            return fail(QStringLiteral("cannot write %1").arg(QDir::toNativeSeparators(file)));
        if (written)
            *written << file;
    }
    return true;
}

} // namespace decolog::core::qsldesign
