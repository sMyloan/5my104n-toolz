#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#include "MainWindow.h"
#include "I18N.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        const bool wayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        || qEnvironmentVariable("XDG_SESSION_TYPE").compare("wayland", Qt::CaseInsensitive) == 0;
        if (!wayland) qputenv("QT_QPA_PLATFORM", "xcb");
    }
    qputenv("QT_STYLE_OVERRIDE", "fusion");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    
    if (geteuid() != 0) {
        //переведено предупреждение о запуске от root
        QMessageBox::warning(nullptr, "Error", "Run with sudo:\nsudo ./5my104n_toolz");
        return 1;
    }
    
    MainWindow w;
    w.show();
    return app.exec();
}
