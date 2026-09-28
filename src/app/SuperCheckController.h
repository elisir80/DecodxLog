// DecoDXLog — Super Check Partial per il QML: il file MASTER.SCP (scaricato da
// supercheckpartial.com quando lo si chiede) piu' i nominativi del log.
#pragma once

#include "core/SuperCheck.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <functional>

class QNetworkAccessManager;

namespace decolog::core { class LogDatabase; }

namespace decolog::app {

class SuperCheckController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY changed)
    Q_PROPERTY(QString fileDate READ fileDate NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
    };

    explicit SuperCheckController(Context context, QObject* parent = nullptr);

    // A log aperto: legge il file salvato e i nominativi del log.
    void start();
    void logChanged() { m_reload.start(); }

    int count() const { return m_scp.size(); }
    int fileCount() const { return m_fileCount; }
    QString fileDate() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

    Q_INVOKABLE QStringList partial(const QString& fragment, int limit = 40) const;
    Q_INVOKABLE QStringList nPlusOne(const QString& call, int limit = 12) const;
    Q_INVOKABLE bool known(const QString& call) const { return m_scp.contains(call); }
    // Scarica MASTER.SCP da supercheckpartial.com.
    Q_INVOKABLE void download();
    // Per i test: il contenuto di un MASTER.SCP.
    void loadData(const QByteArray& data);
    // Per le schermate di prova: qualche nominativo come se fosse il file.
    Q_INVOKABLE void loadTestCalls(const QStringList& calls) { loadData(calls.join(QStringLiteral("\n")).toLatin1()); }
    void setUrl(const QString& url) { m_url = url; }
    static QString filePath();

signals:
    void changed();

private:
    void rebuild();

    Context m_ctx;
    core::SuperCheck m_scp;
    QByteArray m_fileData;
    int m_fileCount{0};
    QDateTime m_fileTime;
    QNetworkAccessManager* m_net{nullptr};
    QString m_url{QStringLiteral("https://www.supercheckpartial.com/MASTER.SCP")};
    bool m_busy{false};
    QString m_status;
    QTimer m_reload;
    // Su un log in un file l'elenco si rifa' su un altro filo: su un milione
    // di QSO sono secondi, e dopo ogni QSO la finestra si fermava.
    QThreadPool m_pool;
    int m_generation{0};
};

} // namespace decolog::app
