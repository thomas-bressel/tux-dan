#pragma once

#include <vector>

#include <QDialog>
#include <QString>

#include "cartridge.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QTableWidget;
class QToolBar;
namespace tuxape {
class PluginHost;
}

namespace tuxdan {

// TuxDAN's window: a cartridge for the Dandanator mini made of games, as
// its menu will list them.
//
// Along the top, a bar of pictograms: open a cartridge TuxDAN made, save
// it, add snapshots or ROMs, add the game being played in TuxAPE, take one
// out, move it up or down, save it as a snapshot, and try the cartridge in
// TuxAPE. Under it, the title of the menu, then the games: their names as
// the menu shows them (to be changed in place), what they are, the machine
// they were taken on, what they take, and the one that starts by itself.
// Below, a gauge of the room they take on the cartridge.
//
// What a button does by asking (a file) is also there as a function that
// is told: the tests use those.
class DanWindow : public QDialog {
    Q_OBJECT

public:
    // `host` is TuxAPE: the game being played comes from it, and the
    // cartridge is tried in it. It may be null, as when a test shows the
    // window by itself.
    explicit DanWindow(tuxape::PluginHost* host, QWidget* parent = nullptr);

    // A snapshot (.sna) or a ROM (.rom, .bin, 16K at most) added at the
    // end; a cartridge (.des) opened. False, the user told why, if it
    // cannot be.
    bool addFile(const QString& path);
    bool addPlaying();
    bool openCartridge(const QString& path);
    bool saveCartridge(const QString& path);
    bool tryCartridge();
    bool exportSnapshot(int row, const QString& path);
    void removeGame(int row);
    void moveGame(int row, int by);
    void setAutoboot(int row);

    Cartridge cartridge() const;
    // What the games take, and the room there is.
    size_t used() const;
    QString status() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    tuxape::PluginHost* host_;
    std::vector<Game> games_;
    std::vector<size_t> sizes_;  // each game's, worked out once
    int autoboot_ = -1;
    QString path_;  // the cartridge's file, once opened or saved

    QToolBar* tools_ = nullptr;
    QLineEdit* title_ = nullptr;
    QTableWidget* table_ = nullptr;
    QProgressBar* room_ = nullptr;
    QLabel* roomText_ = nullptr;
    QLabel* status_ = nullptr;

    void addGame(Game game);
    void showGames();
    void showRoom();
    void say(const QString& text, bool problem = false);
    int selectedRow() const;
    void chooseFiles();
    void chooseCartridge();
    void chooseSave();
    void chooseExport();
};

}  // namespace tuxdan
