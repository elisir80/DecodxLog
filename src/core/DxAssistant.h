// DecoDXLog — ranking DX spiegabile, senza effetti collaterali radio.
#pragma once

#include <QDateTime>
#include <QList>
#include <QStringList>

namespace decolog::core::dxassistant {

struct Candidate {
    QString call;
    QString band;
    QString mode;
    QDateTime spottedAt;
    int status{0};
    int propagationQuality{-1}; // -1 = previsione assente, 0..3
    bool hasPropagation{false};
    bool hasHistory{false};
    QString source;
};

struct Preferences {
    int newEntityWeight{40};
    int newBandWeight{25};
    int newModeWeight{15};
    int newReferenceWeight{20};
    int propagationWeight{20};
    int freshnessMinutes{30};
};

struct Recommendation {
    Candidate candidate;
    double score{0.0};
    QStringList reasons;
    QStringList limitations;
};

// Classifica esclusivamente candidati forniti dal chiamante. Non esegue CAT,
// non modifica il log e non trasmette.
QList<Recommendation> rank(const QList<Candidate>& candidates,
                           const Preferences& preferences = {},
                           const QDateTime& now = QDateTime::currentDateTimeUtc());

} // namespace decolog::core::dxassistant
