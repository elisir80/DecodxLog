// DecoDXLog — le QSL di carta.
//
// Tre stati e basta: da mandare, mandata, ricevuta. Il programma tiene la coda e
// stampa le etichette; la busta la fa l'operatore. Una etichetta raccoglie fino
// a quattro QSO con lo stesso corrispondente, perche' una QSL sola risponde a
// tutti.
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
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

signals:
    void changed();

private:
    void note(const QString& text, const QString& level);
    void touch(qint64 id, const QString& sent, const QString& rcvd, const QString& via);

    Context m_ctx;
    QString m_lastFile;
    QString m_status;
};

} // namespace decolog::app
