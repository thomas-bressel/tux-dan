#include "danwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include "contrast.h"
#include "icons.h"
#include "plugin.h"

namespace tuxdan {

namespace {

enum Column { kName, kKind, kMachine, kSize, kAutoboot, kColumns };

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeAll(const QString& path, const std::vector<uint8_t>& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
           && file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()))
                  == static_cast<qint64>(bytes.size());
}

std::span<const uint8_t> bytesOf(const QByteArray& data)
{
    return {reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size())};
}

// What the engine says, in the window's language.
[[maybe_unused]] const char* const kEngineTexts[] = {
    QT_TRANSLATE_NOOP("Engine", "a ROM is not of 16K"),
    QT_TRANSLATE_NOOP("Engine", "a cartridge has room in its menu for 31 games"),
    QT_TRANSLATE_NOOP("Engine", "a snapshot has neither 64K nor 128K of memory"),
    QT_TRANSLATE_NOOP("Engine", "the games do not fit on the cartridge"),
    QT_TRANSLATE_NOOP("Engine", "the memory of a game is damaged"),
    QT_TRANSLATE_NOOP("Engine", "this cartridge was made by another version of TuxDAN"),
    QT_TRANSLATE_NOOP("Engine", "this cartridge was not made by TuxDAN"),
    QT_TRANSLATE_NOOP("Engine", "this is a Plus's snapshot: the Dandanator's loader cannot put back its ASIC"),
    QT_TRANSLATE_NOOP("Engine", "this is not a snapshot file"),
    QT_TRANSLATE_NOOP("Engine", "this snapshot has more than 128K of memory"),
    QT_TRANSLATE_NOOP("Engine", "this snapshot has the RAM in one of CP/M's configurations, which the loader cannot put back"),
    QT_TRANSLATE_NOOP("Engine", "this snapshot holds no memory"),
    QT_TRANSLATE_NOOP("Engine", "this snapshot is of a version that is not known"),
    QT_TRANSLATE_NOOP("Engine", "this snapshot's memory is damaged"),
};

QString engineText(const std::string& text)
{
    return QCoreApplication::translate("Engine", text.c_str());
}

QString kilobytes(size_t bytes)
{
    return QString::number((bytes + 1023) / 1024) + "K";
}

}  // namespace

DanWindow::DanWindow(tuxape::PluginHost* host, QWidget* parent)
    : QDialog(parent)
    , host_(host)
{
    setObjectName("TuxDanWindow");
    setWindowTitle(tr("TuxDAN %1 – Dandanator Cartridge Maker").arg(TUXDAN_VERSION));
    setAcceptDrops(true);
    resize(760, 520);

    tools_ = new QToolBar(this);
    tools_->setObjectName("tbTools");
    tools_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    const auto tool = [this](Icon id, const QString& text, const char* name, auto slot) {
        QAction* action = tools_->addAction(text);
        action->setObjectName(name);
        setIcon(action, id);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    tool(Icon::Open, tr("Open"), "aOpen", &DanWindow::chooseCartridge)->setToolTip(tr("Open a cartridge TuxDAN made"));
    tool(Icon::Save, tr("Save"), "aSave", &DanWindow::chooseSave)->setToolTip(tr("Save the cartridge as a .des file"));
    tools_->addSeparator();
    tool(Icon::Add, tr("Add"), "aAdd", &DanWindow::chooseFiles)->setToolTip(tr("Add snapshots (.sna) or ROMs of 16K"));
    tool(Icon::Playing, tr("Add Game Played"), "aPlaying", [this] { addPlaying(); })
        ->setToolTip(tr("Add the game being played in TuxAPE, as it is now"));
    tool(Icon::Remove, tr("Remove"), "aRemove", [this] { removeGame(selectedRow()); })->setToolTip(tr("Take the game out"));
    tool(Icon::Up, tr("Up"), "aUp", [this] { moveGame(selectedRow(), -1); })->setToolTip(tr("Move the game up the menu"));
    tool(Icon::Down, tr("Down"), "aDown", [this] { moveGame(selectedRow(), 1); })->setToolTip(tr("Move the game down the menu"));
    tool(Icon::Export, tr("Export"), "aExport", &DanWindow::chooseExport)->setToolTip(tr("Save the game as a snapshot"));
    tools_->addSeparator();
    tool(Icon::Try, tr("Try in TuxAPE"), "aTry", [this] { tryCartridge(); })
        ->setToolTip(tr("Put the cartridge in TuxAPE's Dandanator and start the machine on it"));

    title_ = new QLineEdit(QStringLiteral("TuxDAN"));
    title_->setObjectName("edTitle");
    title_->setMaxLength(Cartridge::kTitleLength);
    auto* titleLabel = new QLabel(tr("Menu &title:"));
    titleLabel->setBuddy(title_);

    table_ = new QTableWidget(0, kColumns);
    table_->setObjectName("twGames");
    table_->setHorizontalHeaderLabels({tr("Name in the menu"), tr("Kind"), tr("Machine"), tr("Size"), tr("Starts by itself")});
    table_->horizontalHeader()->setSectionResizeMode(kName, QHeaderView::Stretch);
    for (int column = kKind; column < kColumns; ++column)
        table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        const int row = item->row();
        if (row < 0 || row >= static_cast<int>(games_.size()))
            return;
        if (item->column() == kName) {
            // As the menu can show it.
            const std::string name = menuText(item->text().toStdString(), Cartridge::kNameLength);
            games_[static_cast<size_t>(row)].name = name;
            if (item->text() != QString::fromStdString(name)) {
                const QSignalBlocker block(table_);
                item->setText(QString::fromStdString(name));
            }
        } else if (item->column() == kAutoboot) {
            setAutoboot(item->checkState() == Qt::Checked ? row : (autoboot_ == row ? -1 : autoboot_));
        }
    });

    room_ = new QProgressBar;
    room_->setObjectName("pbRoom");
    room_->setTextVisible(false);
    roomText_ = new QLabel;
    roomText_->setObjectName("lRoom");
    status_ = new QLabel;
    status_->setObjectName("lStatus");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(titleLabel);
    titleRow->addWidget(title_, 1);
    auto* roomRow = new QHBoxLayout;
    roomRow->addWidget(room_, 1);
    roomRow->addWidget(roomText_);
    auto* layout = new QVBoxLayout(this);
    layout->setMenuBar(tools_);
    layout->addLayout(titleRow);
    layout->addWidget(table_, 1);
    layout->addLayout(roomRow);
    layout->addWidget(status_);
    showGames();
    say(tr("Add snapshots or ROMs, or the game being played, or drop their files here."));
}

void DanWindow::say(const QString& text, bool problem)
{
    status_->setText(text);
    // A problem shows by its weight as well as its colour.
    QFont font = status_->font();
    font.setBold(problem);
    status_->setFont(font);
    QPalette colours = status_->palette();
    const QColor window = palette().color(QPalette::Window);
    colours.setColor(QPalette::WindowText, problem ? readable(QColor(0xB0, 0x20, 0x20), window)
                                                   : palette().color(QPalette::WindowText));
    status_->setPalette(colours);
}

QString DanWindow::status() const
{
    return status_->text();
}

int DanWindow::selectedRow() const
{
    const auto rows = table_->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

Cartridge DanWindow::cartridge() const
{
    Cartridge made;
    made.title = menuText(title_->text().toStdString(), Cartridge::kTitleLength);
    made.games = games_;
    made.autoboot = autoboot_;
    return made;
}

size_t DanWindow::used() const
{
    size_t total = 0;
    for (const size_t size : sizes_)
        total += size;
    return total;
}

void DanWindow::addGame(Game game)
{
    sizes_.push_back(gameSize(game));
    games_.push_back(std::move(game));
    showGames();
    table_->selectRow(static_cast<int>(games_.size()) - 1);
}

bool DanWindow::addFile(const QString& path)
{
    const QString name = QFileInfo(path).completeBaseName();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "des")
        return openCartridge(path);
    const QByteArray data = readAll(path);
    if (data.isEmpty()) {
        say(tr("Cannot read %1.").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    if (static_cast<int>(games_.size()) >= Cartridge::kMaxGames) {
        say(tr("The menu has room for %1 games.").arg(Cartridge::kMaxGames), true);
        return false;
    }
    Game game;
    game.name = menuText(name.toStdString(), Cartridge::kNameLength);
    if (data.startsWith("MV - SNA")) {
        std::string error;
        const auto snapshot = readSnapshot(bytesOf(data), &error);
        if (!snapshot) {
            say(tr("%1: %2.").arg(name, engineText(error)), true);
            return false;
        }
        game.snapshot = *snapshot;
    } else if (data.size() <= static_cast<qsizetype>(Cartridge::kSlotSize)) {
        // A ROM, or a part of one: the rest erased.
        game.kind = Game::Kind::Rom;
        game.rom.assign(data.begin(), data.end());
        game.rom.resize(Cartridge::kSlotSize, 0xFF);
    } else if (data.size() == static_cast<qsizetype>(Cartridge::kSlotSize) + 128) {
        // A ROM with an AMSDOS header, taken off.
        game.kind = Game::Kind::Rom;
        game.rom.assign(data.begin() + 128, data.end());
    } else {
        say(tr("%1 is neither a snapshot nor a ROM of 16K.").arg(name), true);
        return false;
    }
    addGame(std::move(game));
    say(tr("%1 added.").arg(name));
    return true;
}

bool DanWindow::addPlaying()
{
    if (!host_) {
        say(tr("TuxAPE is not there to give its game."), true);
        return false;
    }
    const QString path = QDir(host_->dataFolder()).filePath("playing.sna");
    if (!host_->saveSnapshot(path)) {
        say(tr("TuxAPE could not save its game as a snapshot."), true);
        return false;
    }
    const QByteArray data = readAll(path);
    QFile::remove(path);
    std::string error;
    const auto snapshot = readSnapshot(bytesOf(data), &error);
    if (!snapshot) {
        say(tr("The game being played cannot go on a cartridge: %1.").arg(engineText(error)), true);
        return false;
    }
    if (static_cast<int>(games_.size()) >= Cartridge::kMaxGames) {
        say(tr("The menu has room for %1 games.").arg(Cartridge::kMaxGames), true);
        return false;
    }
    Game game;
    game.name = tr("Game %1").arg(games_.size() + 1).toStdString();
    game.snapshot = *snapshot;
    addGame(std::move(game));
    say(tr("The game being played is added: give it its name in the list."));
    table_->editItem(table_->item(static_cast<int>(games_.size()) - 1, kName));
    return true;
}

bool DanWindow::openCartridge(const QString& path)
{
    const QByteArray data = readAll(path);
    std::string error;
    const auto read = readCartridge(bytesOf(data), &error);
    if (!read) {
        say(tr("%1: %2.").arg(QFileInfo(path).fileName(), engineText(error)), true);
        return false;
    }
    games_ = read->games;
    sizes_.clear();
    for (const Game& game : games_)
        sizes_.push_back(gameSize(game));
    autoboot_ = read->autoboot;
    title_->setText(QString::fromStdString(read->title));
    path_ = path;
    showGames();
    say(tr("%1 opened.").arg(QFileInfo(path).fileName()));
    return true;
}

bool DanWindow::saveCartridge(const QString& path)
{
    std::string error;
    const auto image = buildCartridge(cartridge(), &error);
    if (image.empty()) {
        say(tr("The cartridge cannot be made: %1.").arg(engineText(error)), true);
        return false;
    }
    if (!writeAll(path, image)) {
        say(tr("Cannot write %1.").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    path_ = path;
    say(tr("Saved as %1.").arg(QDir::toNativeSeparators(path)));
    return true;
}

bool DanWindow::tryCartridge()
{
    if (!host_) {
        say(tr("TuxAPE is not there to try the cartridge in."), true);
        return false;
    }
    if (games_.empty()) {
        say(tr("The cartridge has no game yet."), true);
        return false;
    }
    // A file of its own: TuxAPE keeps there what the machine writes to the
    // flash, which is not to change the user's.
    const QString path = QDir(host_->dataFolder()).filePath("try.des");
    std::string error;
    const auto image = buildCartridge(cartridge(), &error);
    if (image.empty()) {
        say(tr("The cartridge cannot be made: %1.").arg(engineText(error)), true);
        return false;
    }
    if (!writeAll(path, image)) {
        say(tr("Cannot write %1.").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    const QString problem = host_->insertDandanator(path);
    if (!problem.isEmpty()) {
        say(problem, true);
        return false;
    }
    say(tr("The cartridge is in TuxAPE's Dandanator: its menu is on the screen."));
    return true;
}

bool DanWindow::exportSnapshot(int row, const QString& path)
{
    if (row < 0 || row >= static_cast<int>(games_.size()) || games_[static_cast<size_t>(row)].kind != Game::Kind::Snapshot) {
        say(tr("Choose a snapshot in the list."), true);
        return false;
    }
    if (!writeAll(path, writeSnapshot(games_[static_cast<size_t>(row)].snapshot))) {
        say(tr("Cannot write %1.").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    say(tr("Saved as %1.").arg(QDir::toNativeSeparators(path)));
    return true;
}

void DanWindow::removeGame(int row)
{
    if (row < 0 || row >= static_cast<int>(games_.size()))
        return;
    games_.erase(games_.begin() + row);
    sizes_.erase(sizes_.begin() + row);
    if (autoboot_ == row)
        autoboot_ = -1;
    else if (autoboot_ > row)
        --autoboot_;
    showGames();
    if (!games_.empty())
        table_->selectRow(std::min(row, static_cast<int>(games_.size()) - 1));
}

void DanWindow::moveGame(int row, int by)
{
    const int to = row + by;
    if (row < 0 || to < 0 || row >= static_cast<int>(games_.size()) || to >= static_cast<int>(games_.size()))
        return;
    std::swap(games_[static_cast<size_t>(row)], games_[static_cast<size_t>(to)]);
    std::swap(sizes_[static_cast<size_t>(row)], sizes_[static_cast<size_t>(to)]);
    if (autoboot_ == row)
        autoboot_ = to;
    else if (autoboot_ == to)
        autoboot_ = row;
    showGames();
    table_->selectRow(to);
}

void DanWindow::setAutoboot(int row)
{
    autoboot_ = row >= 0 && row < static_cast<int>(games_.size()) ? row : -1;
    showGames();
}

void DanWindow::showGames()
{
    const QSignalBlocker block(table_);
    table_->setRowCount(static_cast<int>(games_.size()));
    for (size_t g = 0; g < games_.size(); ++g) {
        const Game& game = games_[g];
        const int row = static_cast<int>(g);
        const auto fixed = [&](int column, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            table_->setItem(row, column, item);
            return item;
        };
        auto* name = new QTableWidgetItem(QString::fromStdString(game.name));
        name->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        table_->setItem(row, kName, name);
        const bool rom = game.kind == Game::Kind::Rom;
        fixed(kKind, rom ? tr("ROM") : tr("Snapshot of %1K").arg(game.snapshot.ram.size() / 1024));
        const int machine = rom ? -1 : game.snapshot.machine();
        fixed(kMachine, machine == 0 ? "CPC 464" : machine == 1 ? "CPC 664" : machine == 2 ? "CPC 6128" : tr("Any"));
        fixed(kSize, kilobytes(sizes_[g]))->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* starts = new QTableWidgetItem;
        if (rom) {
            starts->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        } else {
            starts->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            starts->setCheckState(autoboot_ == row ? Qt::Checked : Qt::Unchecked);
        }
        table_->setItem(row, kAutoboot, starts);
    }
    showRoom();
}

void DanWindow::showRoom()
{
    const size_t total = used();
    room_->setRange(0, static_cast<int>(kRoom / 1024));
    room_->setValue(static_cast<int>(std::min(total, kRoom) / 1024));
    roomText_->setText(total > kRoom ? tr("%1 of %2: too much").arg(kilobytes(total), kilobytes(kRoom))
                                     : tr("%1 of %2").arg(kilobytes(total), kilobytes(kRoom)));
    QFont font = roomText_->font();
    font.setBold(total > kRoom);
    roomText_->setFont(font);
}

void DanWindow::chooseFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Games"), {},
                                                            tr("Snapshots and ROMs (*.sna *.rom *.bin);;All files (*)"));
    for (const QString& file : files)
        addFile(file);
}

void DanWindow::chooseCartridge()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Open Cartridge"), QFileInfo(path_).absolutePath(),
                                                      tr("Dandanator cartridges (*.des *.rom);;All files (*)"));
    if (!file.isEmpty())
        openCartridge(file);
}

void DanWindow::chooseSave()
{
    QString file = QFileDialog::getSaveFileName(this, tr("Save Cartridge"), path_, tr("Dandanator cartridges (*.des)"));
    if (file.isEmpty())
        return;
    if (QFileInfo(file).suffix().isEmpty())
        file += ".des";
    saveCartridge(file);
}

void DanWindow::chooseExport()
{
    const int row = selectedRow();
    if (row < 0 || games_[static_cast<size_t>(row)].kind != Game::Kind::Snapshot) {
        say(tr("Choose a snapshot in the list."), true);
        return;
    }
    const QString name = QString::fromStdString(games_[static_cast<size_t>(row)].name) + ".sna";
    const QString file = QFileDialog::getSaveFileName(this, tr("Export Snapshot"), name, tr("Snapshots (*.sna)"));
    if (!file.isEmpty())
        exportSnapshot(row, file);
}

void DanWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DanWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile())
            addFile(url.toLocalFile());
}

void DanWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        refreshIcons(this);
    QDialog::changeEvent(event);
}

}  // namespace tuxdan
