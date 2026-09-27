// DecoDXLog — l'orologio mondiale: la barra in basso e la finestra grande.
//
// Un solo orologio da un secondo per tutti: l'ora UTC, quella del QTH e delle
// citta' scelte, l'alba e il tramonto, la grayline. Le ore locali vengono dal
// database dei fusi del sistema (QTimeZone), cosi' l'ora legale cambia da
// sola; il Sole lo calcola core::worldclock, senza rete.
//
// Le citta' si scelgono (e si ricordano): quali, quali due stanno nella barra
// in basso, quale e' selezionata nella finestra grande.
#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QLocale>
#include <QObject>
#include <QTimeZone>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace decolog::app {

class WorldClockController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QDateTime now READ now NOTIFY tick)
    Q_PROPERTY(QString utcClock READ utcClock NOTIFY tick)
    Q_PROPERTY(QString utcDate READ utcDate NOTIFY tick)
    Q_PROPERTY(QVariantMap subSolar READ subSolar NOTIFY tick)
    Q_PROPERTY(QVariantList cities READ cities NOTIFY tick)
    Q_PROPERTY(QVariantMap home READ home NOTIFY tick)
    Q_PROPERTY(QVariantList footerCities READ footerCities NOTIFY tick)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY tick)
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY selectionChanged)
    Q_PROPERTY(QStringList footerIds READ footerIds NOTIFY citiesChanged)
    // Si ferma quando la finestra principale e' ridotta a icona.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    struct Context {
        // Il QTH della stazione: locatore e posizione (lat, lon), se ci sono.
        std::function<QString()> stationGrid;
        std::function<QVariantMap()> stationPosition;
    };

    explicit WorldClockController(Context context, QObject* parent = nullptr);

    QDateTime now() const { return m_now; }
    QString utcClock() const;
    QString utcDate() const;
    QVariantMap subSolar() const;
    QVariantList cities() const { return m_cityInfo; }
    QVariantMap home() const;
    QVariantList footerCities() const;
    QVariantMap detail() const;
    QString selected() const { return m_selected; }
    void setSelected(const QString& id);
    QStringList footerIds() const { return m_footer; }
    bool active() const { return m_timer.isActive(); }
    void setActive(bool on);

    // Le citta' che si possono aggiungere: {id, name, short, zone, lat, lon}.
    Q_INVOKABLE QVariantList catalog() const;
    Q_INVOKABLE void addCity(const QString& id);
    Q_INVOKABLE void removeCity(const QString& id);
    // Nella barra in basso ce ne stanno due: questa entra al posto della piu'
    // vecchia, o esce se c'era gia'.
    Q_INVOKABLE void toggleFooter(const QString& id);
    Q_INVOKABLE void resetCities();

    // Per le prove: l'ora ferma a un istante.
    void freezeAt(const QDateTime& utc);

signals:
    void tick();
    void selectionChanged();
    void citiesChanged();
    void activeChanged();

private:
    struct City {
        QString id;
        QString name;
        QString shortName;
        QString zone;
        double lat{0};
        double lon{0};
        bool home{false};
    };

    void update();
    void load();
    void save();
    QList<City> allCities() const;
    QVariantMap infoFor(const City& city) const;
    QString clock(const QDateTime& t, bool seconds) const;
    QString shortDate(const QDate& d) const;

    Context m_ctx;
    QTimer m_timer;
    QDateTime m_now;
    bool m_frozen{false};
    QLocale m_locale;
    QList<City> m_cities;          // le DX, senza il QTH
    QStringList m_footer;
    QString m_selected{QStringLiteral("home")};
    QVariantList m_cityInfo;       // rifatta ogni secondo
};

} // namespace decolog::app
