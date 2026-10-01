#include "core/Config.h"
#include "core/EventQueue.h"
#include "ui/MainMenu.h"
#include "ui/MonitorWindow.h"

#include <QApplication>

#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("honeyden"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    Events::registerMetaTypes();

    Config config;
    if (config.usingDefaults())
        std::cout << "config: using defaults (port 2222)" << std::endl;
    else
        std::cout << "config: loaded from " << config.filePath().toStdString()
                  << " port " << config.port() << std::endl;

    MainMenu menu;
    menu.show();

    // Launch the trap + live table when the animated button fires.
    QObject::connect(&menu, &MainMenu::startRequested, [&]() {
        auto *monitor = new MonitorWindow(config);
        monitor->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(monitor, &MonitorWindow::closed, &menu, [&menu]() {
            menu.show();
        });
        menu.hide();
        monitor->show();
    });

    return app.exec();
}
