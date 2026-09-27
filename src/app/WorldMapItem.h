// DecoDXLog — la mappa del mondo dell'orologio mondiale.
//
// Proiezione equirettangolare (2:1), disegnata in vettoriale con QPainter:
// terre emerse e confini di Natural Earth, reticolo, le quattro fasce della
// notte (tramonto, crepuscolo civile, nautico, astronomico) e la grayline.
// Resta nitida a qualsiasi dimensione e densita' di pixel perche' si ridisegna
// alla risoluzione dello schermo, non si ingrandisce un'immagine.
//
// Niente Canvas Qt Quick e niente texture FBO: e' un QQuickPaintedItem che
// disegna su una QImage, la stessa strada sicura della mappa compatibile.
// Le fasce si ricalcolano solo quando cambia il minuto.
#pragma once

#include "core/WorldClock.h"

#include <QColor>
#include <QDateTime>
#include <QPainterPath>
#include <QQuickPaintedItem>

namespace decolog::app {

class WorldMapItem : public QQuickPaintedItem {
    Q_OBJECT
    // L'istante (UTC) da disegnare: si ridisegna quando cambia il minuto.
    Q_PROPERTY(QDateTime time READ time WRITE setTime NOTIFY timeChanged)
    // true: terre e confini a 1:50m, reticolo e grayline con l'alone; false:
    // terre a 1:110m, per la mappa piccola della barra in basso.
    Q_PROPERTY(bool detailed READ detailed WRITE setDetailed NOTIFY changed)
    Q_PROPERTY(QColor oceanColor MEMBER m_ocean NOTIFY changed)
    Q_PROPERTY(QColor landColor MEMBER m_land NOTIFY changed)
    Q_PROPERTY(QColor coastColor MEMBER m_coast NOTIFY changed)
    Q_PROPERTY(QColor borderColor MEMBER m_border NOTIFY changed)
    Q_PROPERTY(QColor gridColor MEMBER m_grid NOTIFY changed)
    Q_PROPERTY(QColor graylineColor MEMBER m_grayline NOTIFY changed)
    Q_PROPERTY(QColor nightColor MEMBER m_night NOTIFY changed)

public:
    explicit WorldMapItem(QQuickItem* parent = nullptr);

    QDateTime time() const { return m_time; }
    void setTime(const QDateTime& time);
    bool detailed() const { return m_detailed; }
    void setDetailed(bool on);

    void paint(QPainter* painter) override;

signals:
    void timeChanged();
    void changed();

private:
    void recompute();

    QDateTime m_time;
    qint64 m_minute{-1};
    bool m_detailed{true};
    QColor m_ocean{QStringLiteral("#0F1B2D")};
    QColor m_land{QStringLiteral("#2A4262")};
    QColor m_coast{QStringLiteral("#4A6A90")};
    QColor m_border{QStringLiteral("#3A5475")};
    QColor m_grid{QStringLiteral("#89B4D0")};
    QColor m_grayline{QStringLiteral("#00D4FF")};
    QColor m_night{QStringLiteral("#02050B")};
    // Le fasce della notte e la grayline, in coordinate 0..360 x 0..180.
    QList<QPainterPath> m_nightBands;
    QPainterPath m_graylinePath;
};

} // namespace decolog::app
