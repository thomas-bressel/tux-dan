#include "contrast.h"

#include <algorithm>
#include <cmath>

namespace tuxdan {

namespace {

double channel(double value)
{
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

// `colour` moved towards white (`by` > 0) or black (`by` < 0), `by` being
// how far of the way it goes.
QColor moved(const QColor& colour, double by)
{
    const QColor rgb = colour.toRgb();
    const double end = by > 0 ? 255.0 : 0.0;
    const double part = std::abs(by);
    const auto mix = [end, part](int value) { return static_cast<int>(std::lround(value + (end - value) * part)); };
    return QColor(mix(rgb.red()), mix(rgb.green()), mix(rgb.blue()));
}

}  // namespace

double relativeLuminance(const QColor& colour)
{
    const QColor rgb = colour.toRgb();
    return 0.2126 * channel(rgb.redF()) + 0.7152 * channel(rgb.greenF()) + 0.0722 * channel(rgb.blueF());
}

double contrastRatio(const QColor& one, const QColor& other)
{
    const double a = relativeLuminance(one);
    const double b = relativeLuminance(other);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

QColor readable(const QColor& wanted, const QColor& against, double ratio)
{
    if (contrastRatio(wanted, against) >= ratio)
        return wanted.toRgb();
    // Towards the end that is furthest from what it has to stand out from.
    const double way = contrastRatio(Qt::black, against) >= contrastRatio(Qt::white, against) ? -1.0 : 1.0;
    double low = 0.0;
    double high = 1.0;
    for (int step = 0; step < 16; ++step) {
        const double middle = (low + high) / 2;
        if (contrastRatio(moved(wanted, way * middle), against) >= ratio)
            high = middle;
        else
            low = middle;
    }
    return moved(wanted, way * high);
}

}  // namespace tuxdan
