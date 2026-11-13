#include "tuxdanplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "danwindow.h"
#include "icons.h"

namespace tuxdan {

bool TuxDanPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    // TuxDAN's texts in TuxAPE's language, before its first tr(): the
    // translations built into the library. For English, the language of
    // the sources, its plurals alone; English too for a language TuxAPE
    // speaks and the plugin does not.
    const QString translations = QStringLiteral(":/i18n/tuxdan/tuxdan_%1.qm");
    if (translator_.load(translations.arg(host->language())) || translator_.load(translations.arg("en")))
        QCoreApplication::installTranslator(&translator_);
    // One of the keys TuxAPE leaves to plugins (see PLUGINS.md).
    action_ = host->addAction(tr("TuxDAN – Dandanator &Cartridge Maker..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F11));
    host->addButton(action_, cartridgeIcon());
    connect(action_, &QAction::triggered, this, &TuxDanPlugin::showWindow);
    // Qt tells the application when the look changes: its events are
    // watched for that one, which costs a comparison an event.
    qApp->installEventFilter(this);
    return true;
}

void TuxDanPlugin::stop()
{
    // TuxAPE unloads the library next: nothing of it may be left running,
    // nor its translator installed.
    qApp->removeEventFilter(this);
    // The window goes.
    delete window_;
    QCoreApplication::removeTranslator(&translator_);
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxDanPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_ && action_)
        host_->addButton(action_, cartridgeIcon());
    return false;
}

void TuxDanPlugin::showWindow()
{
    if (!window_)
        window_ = new DanWindow(host_, host_->window());
    window_->show();
    window_->raise();
    window_->activateWindow();
}

}  // namespace tuxdan
