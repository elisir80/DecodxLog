// Un popup in una finestra sua (popupType: Popup.Window) riceve i tasti solo se ha
// il fuoco: senza «focus: true» il campo di testo prende il cursore ma quello che
// si scrive si perde. E' successo con la data dello scarico da LoTW, che non si
// poteva scrivere. Qui si guardano i sorgenti QML: ogni popup di questo tipo che
// contiene un campo da scrivere deve dichiarare il fuoco.
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

namespace {

// Cio' che fa scrivere: i campi di testo e i pezzi che li contengono.
const QRegularExpression kTextInputs(
    QStringLiteral(R"(\b(StyledTextField|TextField|TextArea|LotwRangeRow|DatePeriodFields|NumberField)\b)"));

struct Block {
    QString head;   // la riga che apre il blocco, per dire dov'e'
    QString body;   // tutto cio' che sta fra le graffe
};

// I blocchi `{ … }` che contengono `popupType: Popup.Window` come proprieta'
// diretta (non in un blocco annidato): sono i popup. Le graffe nelle stringhe
// non ci sono, nei QML del programma.
QList<Block> windowPopups(QString text)
{
    // Via i commenti: parlano di «focus: true» e di graffe senza esserlo. Il «//» di
    // un indirizzo (https://…) non e' un commento.
    static const QRegularExpression comment(QStringLiteral(R"((?<!:)//[^\n]*)"));
    text.remove(comment);
    QList<Block> found;
    struct Open { qsizetype at; bool direct; };
    QList<Open> stack;
    const QString marker = QStringLiteral("popupType: Popup.Window");
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('{')) {
            stack.append({i, false});
        } else if (c == QLatin1Char('}') && !stack.isEmpty()) {
            const Open open = stack.takeLast();
            if (open.direct) {
                const qsizetype lineStart = text.lastIndexOf(QLatin1Char('\n'), open.at) + 1;
                found.append({text.mid(lineStart, open.at - lineStart + 1).trimmed(),
                              text.mid(open.at + 1, i - open.at - 1)});
            }
        } else if (c == QLatin1Char('p') && !stack.isEmpty() && text.mid(i, marker.size()) == marker) {
            stack.last().direct = true;
        }
    }
    return found;
}

// «focus: true» dichiarato dal popup stesso, non da un campo dentro.
bool declaresFocus(const QString& body)
{
    int depth = 0;
    const QString wanted = QStringLiteral("focus: true");
    for (qsizetype i = 0; i < body.size(); ++i) {
        const QChar c = body.at(i);
        if (c == QLatin1Char('{'))
            ++depth;
        else if (c == QLatin1Char('}'))
            --depth;
        else if (depth == 0 && c == QLatin1Char('f') && body.mid(i, wanted.size()) == wanted
                 && (i == 0 || !body.at(i - 1).isLetterOrNumber()))
            return true;
    }
    return false;
}

} // namespace

class TestQmlPopups : public QObject {
    Q_OBJECT

private slots:
    void windowPopupsWithTextFieldsHaveFocus()
    {
        const QDir dir(QStringLiteral(QML_DIR));
        QVERIFY2(dir.exists(), "la cartella dei QML non c'e'");
        const QStringList files = dir.entryList({QStringLiteral("*.qml")}, QDir::Files);
        QVERIFY(files.size() > 50);
        int popups = 0;
        QStringList missing;
        for (const QString& name : files) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(file.readAll());
            for (const Block& popup : windowPopups(text)) {
                ++popups;
                if (kTextInputs.match(popup.body).hasMatch() && !declaresFocus(popup.body))
                    missing << name + QStringLiteral(": ") + popup.head;
            }
        }
        QVERIFY2(popups >= 5, "non ha trovato i popup: il controllo non guarda piu' niente");
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral("popup in una finestra con un campo di testo e senza «focus: true»:\n")
                            + missing.join(QLatin1Char('\n'))));
    }

    // Il controllo stesso: un popup senza fuoco viene visto, uno col fuoco no.
    void theCheckSeesTheDifference()
    {
        const QString without = QStringLiteral("Item { Popup { popupType: Popup.Window\n modal: true\n"
                                               " contentItem: Item { StyledTextField { focus: true } } } }");
        const QString with = QStringLiteral("Item { Popup { popupType: Popup.Window\n focus: true\n"
                                            " contentItem: Item { StyledTextField { } } } }");
        const QList<Block> a = windowPopups(without);
        QCOMPARE(a.size(), 1);
        QVERIFY(kTextInputs.match(a.first().body).hasMatch());
        QVERIFY(!declaresFocus(a.first().body));   // il focus del campo non e' quello del popup
        const QList<Block> b = windowPopups(with);
        QCOMPARE(b.size(), 1);
        QVERIFY(declaresFocus(b.first().body));
    }
};

QTEST_GUILESS_MAIN(TestQmlPopups)
#include "tst_qmlpopups.moc"
