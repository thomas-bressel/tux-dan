#pragma once

#include <QIcon>

class QAction;
class QWidget;

namespace tuxdan {

// TuxDAN's pictograms, drawn in code on a grid of 20 as TuxAPE draws its
// own, so that they stay sharp at any scale. Their outlines stand out from
// the windows' colour, light look or dark: they are drawn again when the
// look changes.
enum class Icon {
    Open,     // a cartridge opened
    Save,     // saved
    Add,      // files added
    Playing,  // the game being played added
    Remove,
    Up,
    Down,
    Export,   // a game saved as a snapshot
    Try,      // the cartridge put in TuxAPE's Dandanator
};

QIcon icon(Icon id);
// Gives an action its pictogram, and marks it to be drawn again by
// refreshIcons().
void setIcon(QAction* action, Icon id);
void refreshIcons(QWidget* window);

// TuxDAN's pictogram for TuxAPE's control panel: a cartridge with its
// label, in the colour of the windows' text.
QIcon cartridgeIcon();

}  // namespace tuxdan
