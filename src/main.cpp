#include "core/Config.h"
#include "core/EventQueue.h"
#include "ui/MainMenu.h"

#include <QApplication>

#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("honeyden"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    Events::registerMetaTypes();

    // Config is loaded here so stage 6+ can pass it to Listener/DbWriter.
    Config config;
    if (config.usingDefaults())
        std::cout << "config: using defaults (port 2222)" << std::endl;
    else
        std::cout << "config: loaded from " << config.filePath().toStdString()
                  << " port " << config.port() << std::endl;

    MainMenu menu;
    menu.show();

    // Stage 5: the monitor window does not exist yet, acknowledge the click.
    // Full transition (trap start + window switch) lands in stage 6/8.
    QObject::connect(&menu, &MainMenu::startRequested, []() {
        std::cout << "honeyden: start requested (monitor window arrives in stage 6)"
                  << std::endl;
    });

    return app.exec();
}
