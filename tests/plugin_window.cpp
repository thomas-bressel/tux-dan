// The plugin as TuxAPE finds it: the library loaded with QPluginLoader and
// taken only if it is a tuxape::Plugin, started with a host that only notes
// what it is asked, its entry chosen and its window shown, the game being
// played added and the cartridge tried in the host; then the plugin stopped
// and unloaded. Once in English, once in French. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxdan_plugin.png

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QPluginLoader>
#include <QTableWidget>
#include <QTemporaryDir>

#include <cstring>

#include "check.h"
#include "host.h"

namespace {

QDialog* danWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "TuxDanWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

// A snapshot of 64K, as TuxAPE would save the game being played.
QByteArray playingSnapshot()
{
    QByteArray file(0x100, '\0');
    std::memcpy(file.data(), "MV - SNA", 8);
    file[0x10] = 3;
    file[0x21] = char(0xF0);
    file[0x22] = char(0xBF);
    file[0x6B] = 64;
    file[0x6D] = 2;
    QByteArray ram(0x10000, '\0');
    for (int i = 0; i < ram.size(); ++i)
        ram[i] = char(i % 251 < 30 ? i : 0);
    return file + ram;
}

struct Run {
    QString entry;
    QString title;
    QString status;
};

Run runOnce(const QString& language, const QString& folder, const QString& picture)
{
    Run run;
    QPluginLoader loader(QStringLiteral(TUXDAN_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return run;
    }
    CHECK(loader.metaData().value("IID").toString() == QStringLiteral("org.tuxape.Plugin/1"));
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return run;
    {
        Host host(folder);
        host.speaks = language;
        host.playing = playingSnapshot();
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        CHECK(host.actions.size() == 1 && host.pictures == 1);
        CHECK(danWindow() == nullptr);
        if (host.actions.size() == 1) {
            run.entry = host.actions[0]->text();
            CHECK(host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F11));
            host.actions[0]->trigger();
        }
        QDialog* window = danWindow();
        CHECK(window && window->isVisible());
        if (window) {
            run.title = window->windowTitle();
            // The game being played, from the host; the cartridge tried in it.
            auto* playing = window->findChild<QAction*>("aPlaying");
            auto* tryIt = window->findChild<QAction*>("aTry");
            auto* games = window->findChild<QTableWidget*>("twGames");
            CHECK(playing && tryIt && games);
            if (playing && tryIt && games) {
                playing->trigger();
                CHECK_EQ(games->rowCount(), 1);
                tryIt->trigger();
                CHECK_EQ(host.inserted.size(), 1);
                CHECK(host.inserted.value(0).endsWith("try.des"));
                CHECK_EQ(QFile(host.inserted.value(0)).size(), 512 * 1024);
            }
            if (auto* status = window->findChild<QLabel*>("lStatus"))
                run.status = status->text();
            if (!picture.isEmpty())
                window->grab().save(picture);
        }
        plugin->stop();
        CHECK(danWindow() == nullptr);
    }
    CHECK(loader.unload());
    return run;
}

}  // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir folder;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const Run english = runOnce("en", folder.path(), prefix.isEmpty() ? QString() : prefix + "tuxdan_plugin.png");
    CHECK(english.entry == "TuxDAN – Dandanator &Cartridge Maker...");
    CHECK(english.title.startsWith("TuxDAN ") && english.title.endsWith("Dandanator Cartridge Maker"));
    CHECK(english.status == "The cartridge is in TuxAPE's Dandanator: its menu is on the screen.");
    const Run french = runOnce("fr", folder.path(), {});
    CHECK(french.entry == "TuxDAN – Créateur de &cartouches Dandanator...");
    CHECK(french.title.endsWith("Créateur de cartouches Dandanator"));
    CHECK(french.status == "La cartouche est dans le Dandanator de TuxAPE : son menu est à l'écran.");
    return checkSummary("plugin_window");
}
