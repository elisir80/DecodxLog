#include "core/Bands.h"

#include <array>

namespace decolog::core::bands {

namespace {

struct Band {
    const char* name;
    double lowMhz;
    double highMhz;
};

// Enumerazione "Band" di ADIF 3.1.
constexpr std::array kBands{
    Band{"2190m", 0.1357, 0.1378},
    Band{"630m", 0.472, 0.479},
    Band{"560m", 0.501, 0.504},
    Band{"160m", 1.8, 2.0},
    Band{"80m", 3.5, 4.0},
    Band{"60m", 5.06, 5.45},
    Band{"40m", 7.0, 7.3},
    Band{"30m", 10.1, 10.15},
    Band{"20m", 14.0, 14.35},
    Band{"17m", 18.068, 18.168},
    Band{"15m", 21.0, 21.45},
    Band{"12m", 24.89, 24.99},
    Band{"10m", 28.0, 29.7},
    Band{"8m", 40.0, 45.0},
    Band{"6m", 50.0, 54.0},
    Band{"5m", 54.000001, 69.9},
    Band{"4m", 70.0, 71.0},
    Band{"2m", 144.0, 148.0},
    Band{"1.25m", 222.0, 225.0},
    Band{"70cm", 420.0, 450.0},
    Band{"33cm", 902.0, 928.0},
    Band{"23cm", 1240.0, 1300.0},
    Band{"13cm", 2300.0, 2450.0},
    Band{"9cm", 3300.0, 3500.0},
    Band{"6cm", 5650.0, 5925.0},
    Band{"3cm", 10000.0, 10500.0},
    Band{"1.25cm", 24000.0, 24250.0},
    Band{"6mm", 47000.0, 47200.0},
    Band{"4mm", 75500.0, 81000.0},
    Band{"2.5mm", 119980.0, 123000.0},
    Band{"2mm", 134000.0, 149000.0},
    Band{"1mm", 241000.0, 250000.0},
    Band{"submm", 300000.0, 7500000.0},
};

} // namespace

QString fromMhz(double mhz)
{
    for (const auto& b : kBands) {
        if (mhz >= b.lowMhz && mhz <= b.highMhz)
            return QString::fromLatin1(b.name);
    }
    return {};
}

double defaultFrequency(const QString& band, const QString& mode)
{
    struct Plan {
        const char* band;
        double cw, ssb, rtty, ft8, ft4;
    };
    // kHz. In fonia sotto i 10 MHz si lavora in LSB, sopra in USB: la radio
    // lo sceglie da se' quando il modo e' SSB.
    static const Plan plans[] = {
        {"160m", 1830, 1850, 1838, 1840, 1840},
        {"80m", 3530, 3700, 3580, 3573, 3575},
        {"60m", 5354, 5360, 5357, 5357, 5357},
        {"40m", 7030, 7150, 7040, 7074, 7047.5},
        {"30m", 10120, 10120, 10140, 10136, 10140},
        {"20m", 14030, 14200, 14080, 14074, 14080},
        {"17m", 18080, 18130, 18100, 18100, 18104},
        {"15m", 21030, 21250, 21080, 21074, 21140},
        {"12m", 24900, 24940, 24920, 24915, 24919},
        {"10m", 28030, 28500, 28080, 28074, 28180},
        {"6m", 50090, 50150, 50300, 50313, 50318},
        {"2m", 144050, 144300, 144600, 144174, 144170},
    };
    const QString b = band.trimmed().toLower();
    const QString m = mode.trimmed().toUpper();
    for (const Plan& p : plans) {
        if (b != QLatin1String(p.band))
            continue;
        double khz = p.ssb;
        if (m == QLatin1String("CW"))
            khz = p.cw;
        else if (m == QLatin1String("RTTY") || m == QLatin1String("PSK31") || m == QLatin1String("PSK")
                 || m == QLatin1String("JTTY"))
            khz = p.rtty;
        else if (m == QLatin1String("FT8") || m == QLatin1String("FT2"))
            khz = p.ft8;
        else if (m == QLatin1String("FT4"))
            khz = p.ft4;
        return khz / 1000.0;
    }
    return 0.0;
}

QStringList all()
{
    QStringList names;
    for (const auto& b : kBands)
        names << QString::fromLatin1(b.name);
    return names;
}

} // namespace decolog::core::bands
