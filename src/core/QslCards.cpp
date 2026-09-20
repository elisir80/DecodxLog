#include "core/QslCards.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace decolog::core::qslcard {

namespace {

// Le misure sono quelle dei fogli veri, prese dalle schede tecniche; il margine
// alto e' quello del primo rigo di etichette.
const QList<Sheet>& known()
{
    static const QList<Sheet> list{
        Sheet{QStringLiteral("l7160"), QStringLiteral("Avery L7160 · 63,5 × 38,1 mm · 3 × 7"),
              3, 7, 63.5, 38.1, 7.0, 15.1, 2.5, 0.0},
        Sheet{QStringLiteral("l7163"), QStringLiteral("Avery L7163 · 99,1 × 38,1 mm · 2 × 7"),
              2, 7, 99.1, 38.1, 5.0, 15.1, 2.5, 0.0},
        Sheet{QStringLiteral("l7165"), QStringLiteral("Avery L7165 · 99,1 × 67,7 mm · 2 × 4"),
              2, 4, 99.1, 67.7, 5.0, 13.0, 2.5, 0.0},
        Sheet{QStringLiteral("70x36"), QStringLiteral("70 × 36 mm · 3 × 8"),
              3, 8, 70.0, 36.0, 0.0, 4.5, 0.0, 0.0},
    };
    return list;
}

double mmToPx(double mm, int resolution)
{
    return mm / 25.4 * resolution;
}

QString field(const QVariantMap& qso, const char* key)
{
    return qso.value(QLatin1String(key)).toString().trimmed();
}

} // namespace

QList<Sheet> sheets()
{
    return known();
}

Sheet sheetById(const QString& id)
{
    for (const Sheet& s : known()) {
        if (s.id == id)
            return s;
    }
    return known().first();
}

QList<Label> group(const QList<QVariantMap>& qsos, int perLabel)
{
    const int cap = qMax(1, perLabel);
    QList<Label> labels;
    QHash<QString, int> openLabel;      // nominativo -> etichetta ancora da riempire

    for (const QVariantMap& qso : qsos) {
        const QString call = field(qso, "call").toUpper();
        if (call.isEmpty())
            continue;
        Line line;
        line.date = field(qso, "date");
        line.time = field(qso, "time");
        line.band = field(qso, "band");
        line.mode = field(qso, "mode");
        line.rst = field(qso, "rst");
        line.freq = field(qso, "freq");

        int index = openLabel.value(call, -1);
        if (index < 0 || labels[index].lines.size() >= cap) {
            Label label;
            label.call = call;
            label.via = field(qso, "via");
            labels << label;
            index = static_cast<int>(labels.size()) - 1;
            openLabel.insert(call, index);
        }
        if (labels[index].via.isEmpty())
            labels[index].via = field(qso, "via");
        labels[index].lines << line;
    }
    return labels;
}

bool writePdf(const QString& path, const QList<Label>& labels, const Sheet& sheet,
              const QVariantMap& station, bool guides, QString* error)
{
    if (labels.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("QslCards", "No QSL to print");
        return false;
    }

    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(300);
    pdf.setTitle(QCoreApplication::translate("QslCards", "QSL labels"));
    pdf.setCreator(QStringLiteral("DecoDXLog %1").arg(QCoreApplication::applicationVersion()));
    // I margini li mette il foglio di etichette, non la pagina.
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter;
    if (!painter.begin(&pdf)) {
        if (error)
            *error = QCoreApplication::translate("QslCards", "Cannot write %1").arg(QFileInfo(path).fileName());
        return false;
    }

    const int dpi = pdf.resolution();
    const double labelW = mmToPx(sheet.width, dpi);
    const double labelH = mmToPx(sheet.height, dpi);
    const double padding = mmToPx(2.0, dpi);
    const int perPage = qMax(1, sheet.columns * sheet.rows);

    const QString myCall = station.value(QStringLiteral("call")).toString().toUpper();
    const QString myGrid = station.value(QStringLiteral("grid")).toString().toUpper();
    const QString myName = station.value(QStringLiteral("name")).toString();
    const QString message = station.value(QStringLiteral("message")).toString();

    QFont head(QStringLiteral("Arial"));
    head.setBold(true);
    QFont body(QStringLiteral("Arial"));
    QFont small(QStringLiteral("Arial"));

    for (int i = 0; i < labels.size(); ++i) {
        if (i > 0 && i % perPage == 0)
            pdf.newPage();
        const int slot = i % perPage;
        const int column = slot % sheet.columns;
        const int row = slot / sheet.columns;
        const double x = mmToPx(sheet.left + column * (sheet.width + sheet.hGap), dpi);
        const double y = mmToPx(sheet.top + row * (sheet.height + sheet.vGap), dpi);
        const QRectF cell(x, y, labelW, labelH);

        if (guides) {
            painter.setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
            painter.drawRect(cell);
        }
        // Un nominativo lungo o un manager non devono finire sull'etichetta
        // accanto: quello che esce dal riquadro si taglia.
        painter.setClipRect(cell);

        const Label& label = labels[i];
        const QRectF inner = cell.adjusted(padding, padding, -padding, -padding);
        const int lineCount = qMax(1, static_cast<int>(label.lines.size()));
        // Tre righe fisse (intestazione, colonne, saluti) piu' i QSO: il corpo si
        // adatta all'altezza dell'etichetta, cosi' lo stesso codice serve fogli
        // grandi e piccoli.
        const double lineH = inner.height() / (lineCount + 3.4);
        const int bodySize = qMax(4, static_cast<int>(lineH * 72.0 / dpi * 0.82));

        painter.setPen(QPen(Qt::black));
        head.setPixelSize(static_cast<int>(lineH * 1.15));
        body.setPointSize(bodySize);
        small.setPointSize(qMax(4, static_cast<int>(bodySize * 0.85)));

        double cursor = inner.top();
        painter.setFont(head);
        const QString to = QCoreApplication::translate("QslCards", "To radio %1").arg(label.call);
        painter.drawText(QRectF(inner.left(), cursor, inner.width(), lineH * 1.2),
                         Qt::AlignLeft | Qt::AlignVCenter, to);
        if (!label.via.isEmpty()) {
            painter.setFont(small);
            painter.drawText(QRectF(inner.left(), cursor, inner.width(), lineH * 1.2),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QCoreApplication::translate("QslCards", "via %1").arg(label.via));
        }
        cursor += lineH * 1.3;

        // Le colonne: data, ora, banda, modo, RST. Larghezze in frazioni della
        // etichetta, cosi' restano allineate su qualsiasi foglio.
        const double w = inner.width();
        const QList<double> widths{0.30, 0.15, 0.18, 0.20, 0.17};
        const QStringList headers{QCoreApplication::translate("QslCards", "Date"),
                                  QCoreApplication::translate("QslCards", "UTC"),
                                  QCoreApplication::translate("QslCards", "Band"),
                                  QCoreApplication::translate("QslCards", "Mode"),
                                  QCoreApplication::translate("QslCards", "RST")};
        painter.setFont(small);
        double cx = inner.left();
        for (int c = 0; c < headers.size(); ++c) {
            painter.drawText(QRectF(cx, cursor, w * widths[c], lineH),
                             Qt::AlignLeft | Qt::AlignVCenter, headers[c]);
            cx += w * widths[c];
        }
        cursor += lineH * 0.95;
        painter.setPen(QPen(QColor(120, 120, 120), 1));
        painter.drawLine(QPointF(inner.left(), cursor), QPointF(inner.right(), cursor));
        painter.setPen(QPen(Qt::black));

        painter.setFont(body);
        for (const Line& line : label.lines) {
            const QStringList cells{line.date, line.time, line.band, line.mode, line.rst};
            cx = inner.left();
            for (int c = 0; c < cells.size(); ++c) {
                painter.drawText(QRectF(cx, cursor, w * widths[c], lineH),
                                 Qt::AlignLeft | Qt::AlignVCenter, cells[c]);
                cx += w * widths[c];
            }
            cursor += lineH;
        }

        painter.setFont(small);
        QString footer = message.isEmpty()
            ? QCoreApplication::translate("QslCards", "TNX QSO · 73")
            : message;
        if (!myCall.isEmpty())
            footer += QStringLiteral(" · %1").arg(myCall);
        if (!myGrid.isEmpty())
            footer += QStringLiteral(" · %1").arg(myGrid);
        if (!myName.isEmpty())
            footer += QStringLiteral(" · %1").arg(myName);
        painter.drawText(QRectF(inner.left(), inner.bottom() - lineH, inner.width(), lineH),
                         Qt::AlignLeft | Qt::AlignVCenter, footer);
        painter.setClipping(false);
    }

    painter.end();
    return true;
}

} // namespace decolog::core::qslcard
