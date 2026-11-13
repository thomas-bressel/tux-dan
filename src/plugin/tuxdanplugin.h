#pragma once

#include <QObject>
#include <QPointer>
#include <QTranslator>

#include "plugin.h"

namespace tuxdan {

class DanWindow;

// What TuxAPE loads: it puts TuxDAN's entry in the Plugins menu and its
// button on the control panel, and shows the cartridge maker's window when
// either is chosen.
class TuxDanPlugin : public QObject, public tuxape::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.tuxape.Plugin/1")
    Q_INTERFACES(tuxape::Plugin)

public:
    bool start(tuxape::PluginHost* host, QString* error) override;
    void stop() override;

protected:
    // The application's look has changed: the button's picture is drawn
    // again for it.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    tuxape::PluginHost* host_ = nullptr;
    QAction* action_ = nullptr;
    QPointer<DanWindow> window_;
    QTranslator translator_;

    void showWindow();
};

}  // namespace tuxdan
