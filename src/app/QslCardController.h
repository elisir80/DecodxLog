// DecoDXLog — le QSL di carta.
//
// Tre stati e basta: da mandare, mandata, ricevuta. Il programma tiene la coda e
// stampa le etichette; la busta la fa l'operatore. Una etichetta raccoglie fino
// a quattro QSO con lo stesso corrispondente, perche' una QSL sola risponde a
// tutti.
#pragma once

#include "core/QslDesign.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QSize>
#include <QVariantMap>
#include <functional>

namespace decolog::core {
class LogDatabase;
}

namespace decolog::app {

class QslCardController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList queue READ queue NOTIFY changed)
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY changed)
    Q_PROPERTY(QVariantList sheets READ sheets CONSTANT)
    Q_PROPERTY(QString lastFile READ lastFile NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // La cartolina: il modello (un'immagine) e i campi posati sopra.
    Q_PROPERTY(QString cardTemplate READ cardTemplate NOTIFY cardChanged)
    Q_PROPERTY(QVariantList cardFields READ cardFields NOTIFY cardChanged)
    Q_PROPERTY(QSize cardSize READ cardSize NOTIFY cardChanged)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        std::function<QVariantMap()> station;   // call, grid, name del profilo attivo
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        std::function<void()> logChanged;
    };

    explicit QslCardController(Context context, QObject* parent = nullptr);

    QVariantList queue() const { return rows(QStringLiteral("queue")); }
    QVariantMap counts() const;
    QVariantList sheets() const;
    QString lastFile() const { return m_lastFile; }
    QString status() const { return m_status; }

    // `state`: queue | sent | received | all.
    Q_INVOKABLE QVariantList rows(const QString& state, int limit = 0) const;
    // Mette in coda: `via` e' B (bureau), D (diretta) o E (elettronica).
    Q_INVOKABLE void enqueue(const QVariantList& ids, const QString& via);
    // Tutte le QSL ricevute a cui non si e' ancora risposto.
    Q_INVOKABLE int enqueueUnanswered(const QString& via);
    Q_INVOKABLE void markSent(const QVariantList& ids, const QString& via);
    Q_INVOKABLE void markReceived(qint64 id, bool received);
    // Toglie dalla coda senza dire che e' partita ("I" di ignore in ADIF).
    Q_INVOKABLE void drop(const QVariantList& ids);
    // Scrive il PDF delle etichette e torna il percorso; vuoto se e' andata male.
    Q_INVOKABLE QString writeLabels(const QUrl& file, const QString& sheetId, int perLabel, bool guides);
    Q_INVOKABLE void refresh();

    // ── La cartolina ────────────────────────────────────────────────────────
    QString cardTemplate() const { return m_card.templatePath; }
    QVariantList cardFields() const;
    // La misura del modello in pixel: serve a QML per tenere le proporzioni.
    QSize cardSize() const { return m_cardSize; }

    // Le chiavi che si possono posare, con l'etichetta gia' tradotta.
    Q_INVOKABLE QVariantList cardKeys() const;
    Q_INVOKABLE void setCardTemplate(const QUrl& file);
    Q_INVOKABLE void addCardField(const QString& key);
    // Gli otto campi che stanno su quasi tutte le QSL, gia' nei riquadri dove
    // stanno di solito. Quelli gia' posati si spostano li' invece di sdoppiarsi.
    Q_INVOKABLE void addStandardCardFields();
    // Trascinamento: x e y sono frazioni del modello (0..1).
    Q_INVOKABLE void moveCardField(int index, double x, double y);
    // Solo le chiavi presenti in `props` cambiano: size, bold, color, align, text.
    Q_INVOKABLE void updateCardField(int index, const QVariantMap& props);
    Q_INVOKABLE void removeCardField(int index);
    // Una cartolina di prova: il primo QSO in coda, o uno inventato se la coda
    // e' vuota — senza, non si vedrebbe dove si stanno mettendo i campi.
    Q_INVOKABLE QVariantMap sampleQso() const;
    Q_INVOKABLE QVariantMap stationInfo() const;
    // Scrive le cartoline dei QSO scelti (o di tutta la coda se `ids` e' vuota).
    Q_INVOKABLE QString writeCardsPdf(const QUrl& file, const QVariantList& ids, int perPage);
    Q_INVOKABLE QString writeCardsPng(const QUrl& folder, const QVariantList& ids);

signals:
    void changed();
    void cardChanged();

private:
    void note(const QString& text, const QString& level);
    void touch(qint64 id, const QString& sent, const QString& rcvd, const QString& via);

    void loadCard();
    void saveCard();
    void measureTemplate();
    QList<QVariantMap> chosenQsos(const QVariantList& ids) const;

    Context m_ctx;
    QString m_lastFile;
    QString m_status;
    core::qsldesign::Card m_card;
    QSize m_cardSize;
};

} // namespace decolog::app
