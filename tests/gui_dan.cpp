// The window, driven as a user drives it: games added from their files,
// named, ordered, one to start by itself; the cartridge saved, opened again
// as it was, a game exported; what cannot be done said, and how much room
// the games take. Without TuxAPE behind it. Runs without a display
// (QT_QPA_PLATFORM=offscreen).

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QTableWidget>
#include <QTemporaryDir>

#include <cstring>

#include "check.h"
#include "danwindow.h"

using namespace tuxdan;

namespace {

void write(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size());
}

QByteArray snapshotFile(int kb, uint8_t fill)
{
    QByteArray file(0x100, '\0');
    std::memcpy(file.data(), "MV - SNA", 8);
    file[0x10] = 3;
    file[0x21] = char(0xF0);
    file[0x22] = char(0xBF);
    file[0x6B] = char(kb);
    file[0x6D] = 0;
    QByteArray ram(kb * 1024, char(fill));
    for (int i = 0; i < ram.size(); i += 97)
        ram[i] = char(i);
    return file + ram;
}

}  // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir folder;
    DanWindow window(nullptr);
    window.show();
    auto* table = window.findChild<QTableWidget*>("twGames");
    auto* title = window.findChild<QLineEdit*>("edTitle");
    auto* room = window.findChild<QProgressBar*>("pbRoom");
    auto* roomText = window.findChild<QLabel*>("lRoom");
    auto* status = window.findChild<QLabel*>("lStatus");
    CHECK(table && title && room && roomText && status);
    if (!(table && title && room && roomText && status))
        return checkSummary("gui_dan");
    CHECK(title->text() == "TuxDAN" && table->rowCount() == 0);
    CHECK(roomText->text() == "0K of 480K");

    // Two snapshots and a ROM.
    write(folder.filePath("Small Game.sna"), snapshotFile(64, 0));
    write(folder.filePath("Big Game.sna"), snapshotFile(128, 0x11));
    write(folder.filePath("tests.rom"), QByteArray(16384, char(0xC7)));
    write(folder.filePath("notes.txt"), QByteArray(20000, 'x'));
    CHECK(window.addFile(folder.filePath("Small Game.sna")));
    CHECK(window.addFile(folder.filePath("Big Game.sna")));
    CHECK(window.addFile(folder.filePath("tests.rom")));
    CHECK(status->text() == "tests added." && !status->font().bold());
    CHECK_EQ(table->rowCount(), 3);
    CHECK(table->item(0, 0)->text() == "Small Game");
    CHECK(table->item(0, 1)->text() == "Snapshot of 64K" && table->item(0, 2)->text() == "CPC 464");
    CHECK(table->item(1, 1)->text() == "Snapshot of 128K");
    CHECK(table->item(2, 1)->text() == "ROM" && table->item(2, 2)->text() == "Any");
    CHECK(window.used() > 16384 && room->value() > 16);
    CHECK(roomText->text().endsWith(" of 480K"));
    // What is not a game: said, in bold.
    CHECK(!window.addFile(folder.filePath("notes.txt")));
    CHECK(status->text() == "notes is neither a snapshot nor a ROM of 16K." && status->font().bold());
    CHECK(!window.addFile(folder.filePath("missing.sna")));
    CHECK(status->text().startsWith("Cannot read "));
    // No TuxAPE behind: neither its game nor a try.
    CHECK(!window.addPlaying() && !window.tryCartridge());

    // Named in the list, as the menu can show it.
    table->item(1, 0)->setText(QString("A name\twith a tab, and far too long for the menu's line"));
    CHECK(table->item(1, 0)->text() == "A name?with a tab, and far too ");
    CHECK(window.cartridge().games[1].name == "A name?with a tab, and far too ");
    // One starts by itself; another takes its place; a ROM cannot.
    table->item(0, 4)->setCheckState(Qt::Checked);
    CHECK_EQ(window.cartridge().autoboot, 0);
    table->item(1, 4)->setCheckState(Qt::Checked);
    CHECK_EQ(window.cartridge().autoboot, 1);
    CHECK(table->item(0, 4)->checkState() == Qt::Unchecked);
    CHECK(!(table->item(2, 4)->flags() & Qt::ItemIsUserCheckable));
    // Moved up: the one that starts by itself goes with it.
    window.moveGame(1, -1);
    CHECK(window.cartridge().games[0].name == "A name?with a tab, and far too ");
    CHECK_EQ(window.cartridge().autoboot, 0);
    window.moveGame(0, -1);  // already at the top
    CHECK(window.cartridge().games[0].name == "A name?with a tab, and far too ");
    title->setText("My Games");

    // Saved, and opened again as it was.
    const QString des = folder.filePath("mine.des");
    CHECK(window.saveCartridge(des));
    CHECK_EQ(QFile(des).size(), 512 * 1024);
    const Cartridge saved = window.cartridge();
    DanWindow other(nullptr);
    CHECK(other.openCartridge(des));
    CHECK(other.cartridge() == saved);
    CHECK(other.used() == window.used());
    // A file that is no cartridge of TuxDAN's.
    write(folder.filePath("other.des"), QByteArray(512 * 1024, char(0xFF)));
    CHECK(!other.openCartridge(folder.filePath("other.des")));
    CHECK(other.status() == "other.des: this cartridge was not made by TuxDAN." && other.cartridge() == saved);

    // A game exported: the snapshot it was, but its version.
    CHECK(window.exportSnapshot(1, folder.filePath("out.sna")));
    QFile out(folder.filePath("out.sna"));
    CHECK(out.open(QIODevice::ReadOnly));
    const QByteArray exported = out.readAll();
    const QByteArray original = snapshotFile(64, 0);
    CHECK(exported.size() == original.size() && exported.mid(0x100) == original.mid(0x100));
    CHECK(!window.exportSnapshot(2, folder.filePath("rom.sna")));
    CHECK(window.status() == "Choose a snapshot in the list.");

    // Taken out: the one that starts by itself too.
    window.removeGame(0);
    CHECK_EQ(table->rowCount(), 2);
    CHECK_EQ(window.cartridge().autoboot, -1);

    // Too much for the cartridge: the gauge says so, saving says why.
    for (int i = 0; i < 4; ++i) {
        QByteArray noisy = snapshotFile(128, 0);
        uint32_t seed = static_cast<uint32_t>(i + 1);
        for (int k = 0x100; k < noisy.size(); ++k) {
            seed = seed * 1103515245u + 12345u;
            noisy[k] = char(seed >> 16);
        }
        write(folder.filePath(QString("noise%1.sna").arg(i)), noisy);
        CHECK(window.addFile(folder.filePath(QString("noise%1.sna").arg(i))));
    }
    CHECK(roomText->text().endsWith(": too much") && roomText->font().bold());
    CHECK(!window.saveCartridge(folder.filePath("full.des")));
    CHECK(window.status() == "The cartridge cannot be made: the games do not fit on the cartridge.");
    return checkSummary("gui_dan");
}
