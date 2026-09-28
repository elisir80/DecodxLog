// DecoDXLog — le entita' DXCC con le date, dal cty.xml di Club Log.
//
// Il cty.csv di AD1C dice a che entita' appartiene un prefisso adesso. Per un
// QSO di vent'anni fa non basta: PJ2 nel 2005 erano le Antille Olandesi, entita'
// che non c'e' piu'; certe operazioni speciali hanno un'entita' tutta loro per
// qualche giorno; altre l'ARRL non le ha mai accettate. Club Log tiene tutto
// questo con le date, in un file che si scarica con la chiave API di Club Log:
//
//   <clublog date="...">
//     <entities>   <entity><adif>1</adif><name>CANADA</name><prefix>VE</prefix>
//                  <deleted>FALSE</deleted><cqz>5</cqz><cont>NA</cont>
//                  <start>...</start><end>...</end></entity> ...
//     <exceptions> <exception record="1"><call>...</call><entity>...</entity>
//                  <adif>...</adif><cqz>...</cqz><cont>...</cont>
//                  <start>...</start><end>...</end></exception> ...
//     <prefixes>   <prefix record="1"><call>VE</call>... come sopra</prefix> ...
//     <invalid_operations> <invalid record="1"><call>...</call><start/><end/></invalid>
//     <zone_exceptions> <zone_exception record="1"><call>..</call><zone>..</zone>...
//
// La ricerca e' quella di Club Log: prima il nominativo esatto fra le eccezioni
// e le operazioni non valide, poi il prefisso piu' lungo valido in quella data.
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

namespace decolog::core {

struct CtyEntity {
    int adif{0};
    QString name;
    QString prefix;
    bool deleted{false};
    int cqz{0};
    QString cont;
    QDateTime start;
    QDateTime end;
};

struct CtyMatch {
    bool found{false};
    // Un'operazione che per il DXCC non vale (licenza non riconosciuta, pirata).
    bool invalid{false};
    // /MM e /AM: niente entita'.
    bool noEntity{false};
    int adif{0};
    QString name;
    int cqz{0};
    QString cont;
    bool deleted{false};
};

class ClubLogCty {
public:
    // Il file gia' decompresso.
    bool load(const QByteArray& xml, QString* error = nullptr);
    // Un .xml o un .gz (come lo manda Club Log).
    bool loadFile(const QString& path, QString* error = nullptr);
    bool isEmpty() const { return m_entities.isEmpty(); }
    QDateTime date() const { return m_date; }
    int entityCount() const { return static_cast<int>(m_entities.size()); }

    CtyMatch lookup(const QString& call, const QDateTime& when) const;
    const CtyEntity* entity(int adif) const;

    // gzip → dati; vuoto con l'errore se non si riesce.
    static QByteArray gunzip(const QByteArray& gz, QString* error = nullptr);

private:
    struct Rule {
        int adif{0};
        QString entity;
        int cqz{0};
        QString cont;
        QDateTime start;
        QDateTime end;
        bool covers(const QDateTime& when) const;
    };

    const Rule* matchRule(const QList<Rule>& rules, const QDateTime& when) const;
    CtyMatch fromRule(const Rule& r) const;

    QHash<int, CtyEntity> m_entities;
    QHash<QString, QList<Rule>> m_exceptions;
    QHash<QString, QList<Rule>> m_prefixes;
    QHash<QString, QList<Rule>> m_invalid;
    QHash<QString, QList<Rule>> m_zones;
    int m_longestPrefix{0};
    QDateTime m_date;
};

} // namespace decolog::core
