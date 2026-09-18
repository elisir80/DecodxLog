// decodium-ui — il sistema di colori della famiglia Decodium.
//
// Unica fonte dei colori per ogni schermata: in QML non si scrivono colori
// letterali, si legge `Theme.<token>`. I tre temi, le varianti d'accento di
// Darkcodium e le densita' sono quelli di DecodiumThemeManager, valore per
// valore, cosi' un programma aperto accanto a Decodium si legge come lo stesso
// strumento.
#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace decodium::ui {

class ThemeManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON

    Q_PROPERTY(QColor bgDeep         READ bgDeep         NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgMedium       READ bgMedium       NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgLight        READ bgLight        NOTIFY paletteChanged)
    Q_PROPERTY(QColor primaryColor   READ primaryColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentColor    READ accentColor    NOTIFY paletteChanged)
    Q_PROPERTY(QColor warningColor   READ warningColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor errorColor     READ errorColor     NOTIFY paletteChanged)
    Q_PROPERTY(QColor successColor   READ successColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor textPrimary    READ textPrimary    NOTIFY paletteChanged)
    Q_PROPERTY(QColor textSecondary  READ textSecondary  NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassOverlay   READ glassOverlay   NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassBorder    READ glassBorder    NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderColor    READ borderColor    NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderSoft     READ borderSoft     NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelColor     READ panelColor     NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelHeader    READ panelHeader    NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBg     READ rowMatchBg     NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBorder READ rowMatchBorder NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentDim      READ accentDim      NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentDeep     READ accentDeep     NOTIFY paletteChanged)
    Q_PROPERTY(bool   isLightTheme   READ isLightTheme   NOTIFY paletteChanged)

    Q_PROPERTY(QStringList availableThemes   READ availableThemes   CONSTANT)
    Q_PROPERTY(QStringList availableVariants READ availableVariants CONSTANT)
    Q_PROPERTY(QStringList availableDensities READ availableDensities CONSTANT)
    Q_PROPERTY(QString currentTheme  READ currentTheme  WRITE setCurrentTheme  NOTIFY paletteChanged)
    Q_PROPERTY(QString accentVariant READ accentVariant WRITE setAccentVariant NOTIFY paletteChanged)
    Q_PROPERTY(QString density       READ density       WRITE setDensity       NOTIFY densityChanged)

    Q_PROPERTY(bool    customColorsEnabled READ customColorsEnabled WRITE setCustomColorsEnabled NOTIFY paletteChanged)
    Q_PROPERTY(QString customBgColor       READ customBgColor       WRITE setCustomBgColor       NOTIFY paletteChanged)
    Q_PROPERTY(QString customTextColor     READ customTextColor     WRITE setCustomTextColor     NOTIFY paletteChanged)

    // Metrica di densita': righe, testo e intestazioni dei pannelli.
    Q_PROPERTY(int rowHeight   READ rowHeight   NOTIFY densityChanged)
    Q_PROPERTY(int fontSize    READ fontSize    NOTIFY densityChanged)
    Q_PROPERTY(int panelHeight READ panelHeight NOTIFY densityChanged)

    // Nominativi, frequenze, RST e orari: a spaziatura fissa si leggono in
    // colonna a colpo d'occhio.
    Q_PROPERTY(QString monoFamily READ monoFamily CONSTANT)
    Q_PROPERTY(QString uiFamily READ uiFamily CONSTANT)

public:
    explicit ThemeManager(QObject* parent = nullptr);

    // Le impostazioni sono cambiate sotto i piedi — il tema e' arrivato da un
    // altro computer: si rilegge tutto e le schermate si ridipingono.
    Q_INVOKABLE void reload();

    QColor bgDeep() const;
    QColor bgMedium() const;
    QColor bgLight() const;
    QColor primaryColor() const   { return palette().primary; }
    QColor secondaryColor() const { return palette().secondary; }
    QColor accentColor() const;
    QColor warningColor() const   { return palette().warning; }
    QColor errorColor() const     { return palette().error; }
    QColor successColor() const   { return palette().success; }
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor glassOverlay() const   { return palette().glassOverlay; }
    QColor glassBorder() const    { return palette().glassBorder; }
    QColor borderColor() const    { return palette().border; }
    QColor borderSoft() const     { return palette().borderSoft; }
    QColor panelColor() const;
    QColor panelHeader() const;
    QColor rowMatchBg() const     { return palette().rowMatchBg; }
    QColor rowMatchBorder() const { return palette().rowMatchBorder; }
    QColor accentDim() const;
    QColor accentDeep() const;
    bool   isLightTheme() const;

    QStringList availableThemes() const;
    QStringList availableVariants() const;
    QStringList availableDensities() const;

    QString currentTheme() const  { return m_theme; }
    void    setCurrentTheme(const QString& name);
    QString accentVariant() const { return m_variant; }
    void    setAccentVariant(const QString& name);
    QString density() const       { return m_density; }
    void    setDensity(const QString& name);

    bool    customColorsEnabled() const { return m_customEnabled; }
    void    setCustomColorsEnabled(bool enabled);
    QString customBgColor() const       { return m_customBg; }
    void    setCustomBgColor(const QString& hex);
    QString customTextColor() const     { return m_customText; }
    void    setCustomTextColor(const QString& hex);

    int rowHeight() const;
    int fontSize() const;
    int panelHeight() const;

    QString monoFamily() const;
    QString uiFamily() const;

signals:
    void paletteChanged();
    void densityChanged();

private:
    struct Palette {
        QColor bgDeep, bgMedium, bgLight;
        QColor primary, secondary, accent;
        QColor warning, error, success;
        QColor textPrimary, textSecondary;
        QColor glassOverlay, glassBorder, border, borderSoft;
        QColor panel, panelHeader;
        QColor rowMatchBg, rowMatchBorder;
        bool   isLight;
    };

    const Palette& palette() const;
    void accentTriple(QColor& accent, QColor& dim, QColor& deep) const;
    QColor customBg() const;
    QColor customText() const;
    static QColor elevate(const QColor& base, double factor);
    void store(const QString& key, const QVariant& value) const;

    QString m_theme{QStringLiteral("Ocean Blue")};
    QString m_variant{QStringLiteral("phosphor")};
    QString m_density{QStringLiteral("regular")};
    bool    m_customEnabled{false};
    QString m_customBg;
    QString m_customText;
};

} // namespace decodium::ui
