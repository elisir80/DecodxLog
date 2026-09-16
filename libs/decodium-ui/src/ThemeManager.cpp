#include "ThemeManager.h"

#include <QFontDatabase>
#include <QSettings>

namespace decodium::ui {

namespace {

const QString kOcean    = QStringLiteral("Ocean Blue");
const QString kStellar  = QStringLiteral("Stellar Light");
const QString kDark     = QStringLiteral("Darkcodium");

} // namespace

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    QSettings s;
    s.beginGroup(QStringLiteral("theme"));
    const QString theme = s.value(QStringLiteral("current"), kOcean).toString();
    if (availableThemes().contains(theme))
        m_theme = theme;
    const QString variant = s.value(QStringLiteral("accentVariant"), m_variant).toString();
    if (availableVariants().contains(variant))
        m_variant = variant;
    const QString density = s.value(QStringLiteral("density"), m_density).toString();
    if (availableDensities().contains(density))
        m_density = density;
    m_customEnabled = s.value(QStringLiteral("customEnabled"), false).toBool();
    m_customBg      = s.value(QStringLiteral("customBg")).toString();
    m_customText    = s.value(QStringLiteral("customText")).toString();
}

void ThemeManager::store(const QString& key, const QVariant& value) const
{
    QSettings s;
    s.setValue(QStringLiteral("theme/") + key, value);
}

QStringList ThemeManager::availableThemes() const { return {kOcean, kStellar, kDark}; }

QStringList ThemeManager::availableVariants() const
{
    return {QStringLiteral("phosphor"), QStringLiteral("cyan"),
            QStringLiteral("amber"), QStringLiteral("red")};
}

QStringList ThemeManager::availableDensities() const
{
    return {QStringLiteral("compact"), QStringLiteral("regular"), QStringLiteral("comfy")};
}

// I valori vengono da DecodiumThemeManager. Se cambiano la', cambiano qui: e'
// esattamente la duplicazione che il modulo condiviso deve eliminare.
const ThemeManager::Palette& ThemeManager::palette() const
{
    static const Palette ocean{
        QColor("#0A0F1A"), QColor("#111827"), QColor("#1E2D42"),
        QColor("#4A90E2"), QColor("#00D4FF"), QColor("#00FF88"),
        QColor("#FF8C00"), QColor("#FF5F56"), QColor("#4CAF50"),
        QColor("#E8F4FD"), QColor("#89B4D0"),
        QColor(26, 58, 92, 64), QColor(74, 144, 226, 80),
        QColor(74, 144, 226, 80), QColor(74, 144, 226, 40),
        QColor("#1E2D42"), QColor("#283C57"),
        QColor(0, 255, 136, 38), QColor("#00FF88"),
        false};
    static const Palette stellar{
        QColor("#EDF2F7"), QColor("#E1E9F1"), QColor("#FFFFFF"),
        QColor("#1F76D2"), QColor("#0E9AAE"), QColor("#0E8C6A"),
        QColor("#B5741A"), QColor("#CE4038"), QColor("#0E8C6A"),
        QColor("#0E1A22"), QColor("#5C6E7E"),
        QColor(255, 255, 255, 214), QColor("#CBD8E3"),
        QColor("#C3D2DF"), QColor("#DFE8F0"),
        QColor("#FFFFFF"), QColor("#EAF1F7"),
        QColor(14, 140, 106, 36), QColor("#0E8C6A"),
        true};
    static const Palette dark{
        QColor("#050706"), QColor("#0d1310"), QColor("#182019"),
        QColor("#19ff88"), QColor("#66e6ff"), QColor("#19ff88"),
        QColor("#ffb84a"), QColor("#ff5466"), QColor("#19ff88"),
        QColor("#d6dcd8"), QColor("#6c7872"),
        QColor(13, 19, 16, 160), QColor(31, 42, 34, 200),
        QColor("#182019"), QColor("#1f2a22"),
        QColor("#0d1310"), QColor("#0a0e0c"),
        QColor(25, 255, 136, 28), QColor("#19ff88"),
        false};

    if (m_theme == kStellar) return stellar;
    if (m_theme == kDark)    return dark;
    return ocean;
}

void ThemeManager::accentTriple(QColor& accent, QColor& dim, QColor& deep) const
{
    if (m_variant == QLatin1String("cyan")) {
        accent = QColor("#66e6ff"); dim = QColor("#1b9fcc"); deep = QColor("#04222d");
    } else if (m_variant == QLatin1String("amber")) {
        accent = QColor("#ffb820"); dim = QColor("#a06d10"); deep = QColor("#2e1d04");
    } else if (m_variant == QLatin1String("red")) {
        accent = QColor("#ff5466"); dim = QColor("#a82c3a"); deep = QColor("#2e090f");
    } else {
        accent = QColor("#19ff88"); dim = QColor("#0fa55a"); deep = QColor("#052d1a");
    }
}

QColor ThemeManager::customBg() const
{
    if (!m_customEnabled) return {};
    QColor c(m_customBg);
    return c.isValid() ? c : QColor();
}

QColor ThemeManager::customText() const
{
    if (!m_customEnabled) return {};
    QColor c(m_customText);
    return c.isValid() ? c : QColor();
}

// Su fondo scuro i livelli superiori schiariscono, su fondo chiaro scuriscono:
// i pannelli restano distinguibili qualunque sfondo scelga l'operatore.
QColor ThemeManager::elevate(const QColor& base, double factor)
{
    if (!base.isValid()) return base;
    return base.lightnessF() < 0.5
        ? base.lighter(static_cast<int>(100 + factor * 100))
        : base.darker(static_cast<int>(100 + factor * 45));
}

QColor ThemeManager::bgDeep() const
{
    const QColor c = customBg();
    return c.isValid() ? c : palette().bgDeep;
}

QColor ThemeManager::bgMedium() const
{
    const QColor c = customBg();
    return c.isValid() ? elevate(c, 0.18) : palette().bgMedium;
}

QColor ThemeManager::bgLight() const
{
    const QColor c = customBg();
    return c.isValid() ? elevate(c, 0.40) : palette().bgLight;
}

QColor ThemeManager::panelColor() const
{
    const QColor c = customBg();
    return c.isValid() ? elevate(c, 0.40) : palette().panel;
}

QColor ThemeManager::panelHeader() const
{
    const QColor c = customBg();
    return c.isValid() ? elevate(c, 0.26) : palette().panelHeader;
}

QColor ThemeManager::textPrimary() const
{
    const QColor c = customText();
    return c.isValid() ? c : palette().textPrimary;
}

QColor ThemeManager::textSecondary() const
{
    QColor c = customText();
    if (c.isValid()) {
        c.setAlphaF(0.62f);
        return c;
    }
    return palette().textSecondary;
}

QColor ThemeManager::accentColor() const
{
    if (m_theme == kDark) {
        QColor a, d, p;
        accentTriple(a, d, p);
        return a;
    }
    return palette().accent;
}

QColor ThemeManager::accentDim() const
{
    if (m_theme == kDark) {
        QColor a, d, p;
        accentTriple(a, d, p);
        return d;
    }
    return accentColor().darker(160);
}

QColor ThemeManager::accentDeep() const
{
    if (m_theme == kDark) {
        QColor a, d, p;
        accentTriple(a, d, p);
        return p;
    }
    return accentColor().darker(420);
}

bool ThemeManager::isLightTheme() const
{
    const QColor c = customBg();
    if (c.isValid()) return c.lightnessF() >= 0.5;
    return palette().isLight;
}

void ThemeManager::setCurrentTheme(const QString& name)
{
    if (name == m_theme || !availableThemes().contains(name)) return;
    m_theme = name;
    store(QStringLiteral("current"), name);
    emit paletteChanged();
}

void ThemeManager::setAccentVariant(const QString& name)
{
    if (name == m_variant || !availableVariants().contains(name)) return;
    m_variant = name;
    store(QStringLiteral("accentVariant"), name);
    emit paletteChanged();
}

void ThemeManager::setDensity(const QString& name)
{
    if (name == m_density || !availableDensities().contains(name)) return;
    m_density = name;
    store(QStringLiteral("density"), name);
    emit densityChanged();
}

void ThemeManager::setCustomColorsEnabled(bool enabled)
{
    if (enabled == m_customEnabled) return;
    m_customEnabled = enabled;
    store(QStringLiteral("customEnabled"), enabled);
    emit paletteChanged();
}

void ThemeManager::setCustomBgColor(const QString& hex)
{
    const QString h = hex.trimmed();
    if (h == m_customBg) return;
    m_customBg = h;
    store(QStringLiteral("customBg"), h);
    if (m_customEnabled) emit paletteChanged();
}

void ThemeManager::setCustomTextColor(const QString& hex)
{
    const QString h = hex.trimmed();
    if (h == m_customText) return;
    m_customText = h;
    store(QStringLiteral("customText"), h);
    if (m_customEnabled) emit paletteChanged();
}

int ThemeManager::rowHeight() const
{
    if (m_density == QLatin1String("compact")) return 22;
    if (m_density == QLatin1String("comfy"))   return 30;
    return 26;
}

int ThemeManager::fontSize() const
{
    if (m_density == QLatin1String("compact")) return 11;
    if (m_density == QLatin1String("comfy"))   return 13;
    return 12;
}

int ThemeManager::panelHeight() const
{
    if (m_density == QLatin1String("compact")) return 26;
    if (m_density == QLatin1String("comfy"))   return 38;
    return 30;
}

QString ThemeManager::monoFamily() const
{
    // Una sola famiglia, scelta fra quelle installate: una stringa con la
    // virgola non e' una lista di ripiego per QML, e senza Cascadia Mono si
    // finirebbe su un carattere proporzionale.
    static const QString family = [] {
#ifdef Q_OS_WIN
        const QStringList installed = QFontDatabase::families();
        for (const auto* candidate : {"Cascadia Mono", "Consolas"}) {
            if (installed.contains(QLatin1String(candidate)))
                return QString::fromLatin1(candidate);
        }
#endif
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    return family;
}

} // namespace decodium::ui
