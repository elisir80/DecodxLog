// DecoDXLog — Super Check Partial e N+1, come nei log da contest.
//
// MASTER.SCP e' l'elenco dei nominativi attivi nei contest (supercheckpartial.com):
// mentre si scrive un pezzo di nominativo si vedono quelli veri che lo
// contengono, e con il nominativo intero quelli che differiscono di un
// carattere solo (N+1) — il modo piu' rapido di accorgersi di aver capito
// "DL1ABD" invece di "DL1ABC". Ai nominativi del file si aggiungono quelli del
// proprio log.
#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QStringList>

namespace decolog::core {

class SuperCheck {
public:
    // Il contenuto di MASTER.SCP: un nominativo per riga, '#' per i commenti.
    int load(const QByteArray& scp);
    // I nominativi del proprio log, che contano come quelli del file.
    void addCalls(const QStringList& calls);
    void clear();
    int size() const { return static_cast<int>(m_calls.size()); }
    bool contains(const QString& call) const { return m_set.contains(call.trimmed().toUpper()); }

    // I nominativi che contengono `fragment` (almeno due caratteri; '?' vale un
    // carattere qualsiasi), i piu' corti prima. `limit` 0 = tutti.
    QStringList partial(const QString& fragment, int limit = 40) const;
    // I nominativi a un carattere di distanza (uno cambiato, aggiunto o tolto).
    QStringList nPlusOne(const QString& call, int limit = 20) const;

    static bool oneEditApart(const QString& a, const QString& b);

private:
    void rebuild();

    QSet<QString> m_set;
    QStringList m_calls;        // in ordine alfabetico
    bool m_dirty{false};
};

} // namespace decolog::core
