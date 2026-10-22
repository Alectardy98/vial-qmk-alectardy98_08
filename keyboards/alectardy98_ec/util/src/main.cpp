// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "boardprofile.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#ifndef EC_FIXTURE_DIR
#define EC_FIXTURE_DIR ""
#endif
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("qmk-ec-util");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("qmk-ec-util");
    QCommandLineParser args;
    args.setApplicationDescription("QMK EC calibration and diagnostics; --offline disables all USB access");
    args.addHelpOption();
    args.addVersionOption();
    args.addOption({"board", "Offline board layout preset (class0413_ec, ec_prox, me_ec_uf2); implies --offline", "id"});
    args.addOption({"offline", "Disable HID discovery and use simulation only"});
    args.addOption({"layout", "Load Vial JSON at runtime", "path"});
    args.addOption({"smoke-test", "Run offscreen-compatible launch/telemetry check and exit"});
    args.addOption({"screenshot", "Save a screenshot after simulated telemetry starts", "path"});
    args.process(app);
    bool offline = args.isSet("offline") || args.isSet("smoke-test") || args.isSet("board");
    ec::MainWindow window(nullptr, !offline);
    QString path = args.value("layout");
    if(args.isSet("board")) {
        const ec::BoardProfile *selected=nullptr;
        for(const auto &profile:ec::boardProfiles())if(profile.id==args.value("board"))selected=&profile;
        if(!selected){qCritical()<<"Unknown board preset";return 2;}
        if(path.isEmpty())path=QStringLiteral(EC_FIXTURE_DIR)+"/"+selected->layout;
    }
    if (path.isEmpty() && offline) {
        QStringList candidates = {QDir(QCoreApplication::applicationDirPath())
                                      .filePath("../share/qmk-ec-util/examples/reverseenginering.vial.json"),
                                  QStringLiteral(EC_FIXTURE_DIR) + "/reverseenginering.vial.json"};
        for (const auto &candidate : candidates)
            if (QFileInfo::exists(candidate)) {
                path = candidate;
                break;
            }
    }
    if (!path.isEmpty() && !window.loadLayout(path)) {
        qCritical().noquote() << window.lastError();
        return 2;
    }
    window.show();
    if (args.isSet("smoke-test") || args.isSet("screenshot"))
        QTimer::singleShot(900, &app, [&]() {
            if (!window.healthy()) {
                qCritical() << "GUI smoke check failed";
                app.exit(3);
                return;
            }
            if (args.isSet("screenshot") && !window.grab().save(args.value("screenshot"))) {
                app.exit(4);
                return;
            }
            qInfo() << "GUI running;" << window.matrixCount() << "matrix positions;" << window.physicalCount()
                    << "visible keys; mock telemetry active";
            if (args.isSet("smoke-test"))
                app.quit();
        });
    return app.exec();
}
