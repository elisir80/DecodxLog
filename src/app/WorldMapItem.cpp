#include "app/WorldMapItem.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace decolog::app {

namespace {

// Le terre e i confini si leggono una volta sola, per tutte le mappe.
struct Geometry {
    QPainterPath land;
    QPainterPath coast;
    QPainterPath borders;
};

QPainterPath ringsToPath(const QJsonArray& rings, bool close)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const QJsonValue& v : rings) {
        const QJsonArray ring = v.toArray();
        if (ring.size() < 4)
            continue;
        // (lon, lat) -> (lon + 180, 90 - lat): la mappa e' 360 x 180.
        path.moveTo(ring.at(0).toDouble() + 180.0, 90.0 - ring.at(1).toDouble());
        for (qsizetype i = 2; i + 1 < ring.size(); i += 2)
            path.lineTo(ring.at(i).toDouble() + 180.0, 90.0 - ring.at(i + 1).toDouble());
        if (close)
            path.closeSubpath();
    }
    return path;
}

QJsonObject readJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const Geometry& geometry(bool detailed)
{
    static Geometry fine = [] {
        Geometry g;
        g.land = ringsToPath(readJson(QStringLiteral(":/decolog/map/land50.json")).value(QStringLiteral("rings")).toArray(), true);
        g.coast = g.land;
        g.borders = ringsToPath(readJson(QStringLiteral(":/decolog/map/borders50.json")).value(QStringLiteral("lines")).toArray(), false);
        return g;
    }();
    static Geometry coarse = [] {
        Geometry g;
        g.land = ringsToPath(readJson(QStringLiteral(":/decolog/map/land.json")).value(QStringLiteral("rings")).toArray(), true);
        g.coast = g.land;
        return g;
    }();
    return detailed ? fine : coarse;
}

} // namespace

WorldMapItem::WorldMapItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    connect(this, &WorldMapItem::changed, this, [this] { update(); });
}

void WorldMapItem::setTime(const QDateTime& time)
{
    m_time = time;
    emit timeChanged();
    // Le fasce e la grayline si muovono di un quarto di grado al minuto: si
    // rifanno al cambio del minuto, non a ogni secondo.
    const qint64 minute = time.toSecsSinceEpoch() / 60;
    if (minute == m_minute)
        return;
    m_minute = minute;
    recompute();
    update();
}

void WorldMapItem::setDetailed(bool on)
{
    if (on == m_detailed)
        return;
    m_detailed = on;
    recompute();
    emit changed();
}

void WorldMapItem::recompute()
{
    using namespace core::worldclock;
    m_nightBands.clear();
    m_graylinePath = QPainterPath();
    if (!m_time.isValid())
        return;
    const SubSolar sun = subSolar(m_time.toUTC());
    const double step = m_detailed ? 1.0 : 3.0;
    for (const double h : {kSunset, kCivil, kNautical, kAstronomical}) {
        const Terminator t = terminator(sun, h, step);
        QPainterPath band;
        for (qsizetype i = 0; i < t.boundary.size(); ++i) {
            const QPointF p(t.boundary.at(i).x() + 180.0, 90.0 - t.boundary.at(i).y());
            if (i == 0)
                band.moveTo(p);
            else
                band.lineTo(p);
            if (h == kSunset) {
                if (i == 0)
                    m_graylinePath.moveTo(p);
                else
                    m_graylinePath.lineTo(p);
            }
        }
        // Si chiude verso il polo in ombra.
        if (t.nightNorth) {
            band.lineTo(360.0, 0.0);
            band.lineTo(0.0, 0.0);
        } else {
            band.lineTo(360.0, 180.0);
            band.lineTo(0.0, 180.0);
        }
        band.closeSubpath();
        m_nightBands << band;
    }
}

void WorldMapItem::paint(QPainter* painter)
{
    const QRectF area = boundingRect();
    painter->fillRect(area, m_ocean);
    if (area.width() <= 0 || area.height() <= 0)
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QTransform toScreen = QTransform::fromScale(area.width() / 360.0, area.height() / 180.0);
    const Geometry& g = geometry(m_detailed);

    // Le terre, la costa e i confini.
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_land);
    painter->drawPath(toScreen.map(g.land));
    QPen coast(m_coast, m_detailed ? 0.8 : 0.5);
    coast.setCosmetic(true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(coast);
    painter->drawPath(toScreen.map(g.coast));
    if (m_detailed && !g.borders.isEmpty()) {
        QPen border(m_border, 0.6);
        border.setCosmetic(true);
        painter->setPen(border);
        painter->drawPath(toScreen.map(g.borders));
    }

    // Il reticolo ogni 30°, l'equatore e il meridiano zero un po' piu' forti.
    if (m_detailed) {
        QColor faint = m_grid;
        faint.setAlphaF(0.14);
        QColor strong = m_grid;
        strong.setAlphaF(0.28);
        for (int lon = -150; lon <= 150; lon += 30) {
            QPen pen(lon == 0 ? strong : faint, 1.0);
            pen.setCosmetic(true);
            painter->setPen(pen);
            const double x = (lon + 180.0) * area.width() / 360.0;
            painter->drawLine(QPointF(x, 0), QPointF(x, area.height()));
        }
        for (int lat = -60; lat <= 60; lat += 30) {
            QPen pen(lat == 0 ? strong : faint, 1.0);
            pen.setCosmetic(true);
            painter->setPen(pen);
            const double y = (90.0 - lat) * area.height() / 180.0;
            painter->drawLine(QPointF(0, y), QPointF(area.width(), y));
        }
    }

    // Le quattro fasce della notte, una sopra l'altra: dove e' piu' buio si
    // sommano.
    QColor night = m_night;
    night.setAlphaF(0.26);
    painter->setPen(Qt::NoPen);
    painter->setBrush(night);
    for (const QPainterPath& band : std::as_const(m_nightBands))
        painter->drawPath(toScreen.map(band));

    // La grayline, con il suo alone.
    if (!m_graylinePath.isEmpty()) {
        painter->setBrush(Qt::NoBrush);
        if (m_detailed) {
            QColor halo = m_grayline;
            halo.setAlphaF(0.35);
            QPen glow(halo, 7.0);
            glow.setCosmetic(true);
            glow.setCapStyle(Qt::RoundCap);
            painter->setPen(glow);
            painter->drawPath(toScreen.map(m_graylinePath));
        }
        QPen line(m_grayline, m_detailed ? 1.6 : 1.0);
        line.setCosmetic(true);
        painter->setPen(line);
        painter->drawPath(toScreen.map(m_graylinePath));
    }
}

} // namespace decolog::app
