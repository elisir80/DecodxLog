// DecoDXLog — i file "call history" dei contest, nel formato di N1MM.
//
// Chi fa contest si porta dietro un file con quello che le stazioni hanno
// mandato l'anno prima: il nome, la zona, lo stato, la sezione. N1MM Logger+ ne
// ha fissato il formato, e tutti gli altri lo leggono:
//
//   # commento
//   !!Order!!,Call,Name,Loc1,CQZone,ITUZone,Exch1,State,Sect,
//   W1AW,HIRAM,FN31,5,8,,CT,CT,
//
// La riga !!Order!! dice cosa c'e' in ogni colonna; ce ne possono essere piu'
// d'una, e vale l'ultima. I nomi delle colonne non badano alle maiuscole.
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace decolog::core {

class CallHistory {
public:
    bool load(const QString& path, QString* error = nullptr);
    bool loadText(const QString& text);
    void clear();
    int size() const { return static_cast<int>(m_rows.size()); }
    QString path() const { return m_path; }

    // Le colonne di un nominativo, con i nomi in maiuscolo ("NAME", "CQZONE").
    // Vuoto se il nominativo non c'e'.
    QHash<QString, QString> find(const QString& call) const;
    // Il primo valore non vuoto fra le colonne indicate.
    QString value(const QString& call, const QStringList& columns) const;

private:
    QString m_path;
    QHash<QString, QHash<QString, QString>> m_rows;
};

} // namespace decolog::core
