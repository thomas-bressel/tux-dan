#pragma once

#include <QColor>

namespace tuxdma {

// The colours TuxDMA picks itself are held to the level TuxAPE holds its
// own to, WCAG 2's AAA: text stands out from what is behind it by 7 to 1
// at least, and what is drawn to tell a state by 3 to 1. Those of the
// user's desktop are not the plugin's to choose; its own are fitted to
// them with these, whatever the look, light or dark.

inline constexpr double kTextContrast = 7.0;
inline constexpr double kGraphicContrast = 3.0;

// WCAG's relative luminance, 0 for black to 1 for white, and the contrast
// of two colours, 1 (none) to 21 (black on white).
double relativeLuminance(const QColor& colour);
double contrastRatio(const QColor& one, const QColor& other);

// The colour nearest to `wanted`, of the same hue, that stands out from
// `against` by `ratio` at least: `wanted` itself if it does, darker or
// lighter if not, as far as black or white.
QColor readable(const QColor& wanted, const QColor& against, double ratio = kTextContrast);

}  // namespace tuxdma
