#include "app/RigController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>

namespace decolog::app {

using core::RigControl;

RigController::RigController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("rig/enabled"), false).toBool();
    m_host = s.value(QStringLiteral("rig/host"), QStringLiteral("127.0.0.1")).toString();
    m_port = s.value(QStringLiteral("rig/port"), 4532).toInt();
    m_wpm = s.value(QStringLiteral("cw/wpm"), 24).toInt();
    loadMacros();

    connect(&m_rig, &RigControl::changed, this, &RigController::stateChanged);
    connect(&m_rig, &RigControl::failed, this, [this](const QString& message) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), message, QStringLiteral("warning"));
        emit stateChanged();
    });
    connect(&m_rig, &RigControl::morseSent, this, [this](const QString& text) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), tr("Sent: %1").arg(text), QStringLiteral("info"));
    });
}

void RigController::start()
{
    if (m_enabled)
        connectNow();
}

QString RigController::frequencyLabel() const
{
    const qint64 hz = m_rig.frequencyHz();
    if (hz <= 0)
        return QStringLiteral("—");
    return QLocale().toString(hz / 1000000.0, 'f', 6) + QStringLiteral(" MHz");
}

void RigController::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("rig/enabled"), on);
    if (on)
        connectNow();
    else
        m_rig.disconnectFromRig();
    emit changed();
    emit stateChanged();
}

void RigController::setHost(const QString& host)
{
    const QString clean = host.trimmed();
    if (clean == m_host)
        return;
    m_host = clean;
    QSettings().setValue(QStringLiteral("rig/host"), clean);
    if (m_enabled)
        connectNow();
    emit changed();
}

void RigController::setPort(int port)
{
    if (port == m_port || port <= 0 || port > 65535)
        return;
    m_port = port;
    QSettings().setValue(QStringLiteral("rig/port"), port);
    if (m_enabled)
        connectNow();
    emit changed();
}

void RigController::setWpm(int wpm)
{
    const int clamped = qBound(5, wpm, 60);
    m_wpm = clamped;
    QSettings().setValue(QStringLiteral("cw/wpm"), clamped);
    m_rig.setSpeedWpm(clamped);
    emit stateChanged();
}

void RigController::overrideConnection(const QString& host, int port)
{
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port > 0 ? port : 4532;
    m_enabled = true;
    connectNow();
    emit changed();
}

void RigController::connectNow()
{
    m_rig.connectTo(m_host, static_cast<quint16>(m_port));
    emit stateChanged();
}

void RigController::disconnectNow()
{
    m_rig.disconnectFromRig();
    emit stateChanged();
}

void RigController::tuneTo(qint64 hz, const QString& mode)
{
    if (hz > 0)
        m_rig.setFrequency(hz);
    if (!mode.isEmpty())
        m_rig.setMode(mode);
}

QString RigController::expand(const QString& text, const QVariantMap& context) const
{
    QString out = text;
    const QString mine = m_ctx.stationCallsign ? m_ctx.stationCallsign() : QString();
    auto put = [&out](const QString& token, const QString& value) {
        out.replace(QStringLiteral("{") + token + QStringLiteral("}"), value, Qt::CaseInsensitive);
    };
    put(QStringLiteral("MYCALL"), mine.toUpper());
    put(QStringLiteral("CALL"), context.value(QStringLiteral("call")).toString().toUpper());
    put(QStringLiteral("RST"), context.value(QStringLiteral("rst"), QStringLiteral("599")).toString());
    put(QStringLiteral("NR"), context.value(QStringLiteral("nr")).toString());
    put(QStringLiteral("EXCH"), context.value(QStringLiteral("exch")).toString());
    put(QStringLiteral("NAME"), context.value(QStringLiteral("name")).toString());
    // Quello che resta senza risposta se ne va: in aria non si manda una
    // parentesi graffa.
    static const QRegularExpression leftovers(QStringLiteral("\\{[A-Za-z#]+\\}"));
    out.remove(leftovers);
    return out.simplified();
}

void RigController::sendMacro(int index, const QVariantMap& context)
{
    if (index < 0 || index >= m_macros.size())
        return;
    sendText(m_macros.at(index).toMap().value(QStringLiteral("text")).toString(), context);
}

void RigController::sendText(const QString& text, const QVariantMap& context)
{
    const QString ready = expand(text, context);
    if (ready.isEmpty())
        return;
    m_rig.sendMorse(ready);
}

void RigController::stop()
{
    m_rig.stopMorse();
}

void RigController::setMacro(int index, const QString& label, const QString& text)
{
    if (index < 0 || index >= m_macros.size())
        return;
    QVariantMap macro = m_macros.at(index).toMap();
    macro.insert(QStringLiteral("label"), label.trimmed());
    macro.insert(QStringLiteral("text"), text.trimmed());
    m_macros[index] = macro;
    saveMacros();
    emit macrosChanged();
}

void RigController::resetMacros()
{
    m_macros = defaultMacros();
    saveMacros();
    emit macrosChanged();
}

QVariantList RigController::defaultMacros()
{
    // Quelle di sempre, nell'ordine in cui le tiene ogni log da contest.
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("CQ")},
                    {QStringLiteral("text"), QStringLiteral("CQ TEST {MYCALL} {MYCALL} TEST")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Call")},
                    {QStringLiteral("text"), QStringLiteral("{CALL}")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Exch")},
                    {QStringLiteral("text"), QStringLiteral("{CALL} 5NN {NR}")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("TU")},
                    {QStringLiteral("text"), QStringLiteral("TU {MYCALL} TEST")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("?")},
                    {QStringLiteral("text"), QStringLiteral("?")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("AGN")},
                    {QStringLiteral("text"), QStringLiteral("AGN")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("NR?")},
                    {QStringLiteral("text"), QStringLiteral("NR?")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("73")},
                    {QStringLiteral("text"), QStringLiteral("73 GL")}},
    };
}

void RigController::loadMacros()
{
    const QString raw = QSettings().value(QStringLiteral("cw/macros")).toString();
    const QJsonArray array = QJsonDocument::fromJson(raw.toUtf8()).array();
    QVariantList out;
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        out << QVariantMap{{QStringLiteral("label"), object.value(QStringLiteral("label")).toString()},
                           {QStringLiteral("text"), object.value(QStringLiteral("text")).toString()}};
    }
    m_macros = out.size() == 8 ? out : defaultMacros();
}

void RigController::saveMacros()
{
    QJsonArray array;
    for (const QVariant& value : std::as_const(m_macros)) {
        const QVariantMap macro = value.toMap();
        array.append(QJsonObject{{QStringLiteral("label"), macro.value(QStringLiteral("label")).toString()},
                                 {QStringLiteral("text"), macro.value(QStringLiteral("text")).toString()}});
    }
    QSettings().setValue(QStringLiteral("cw/macros"),
                         QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

} // namespace decolog::app
