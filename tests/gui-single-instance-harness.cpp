// Headless, real Qt local-socket and process-lock integration harness.
// Compiled by check-single-gui.py; no PipeWire backend or ViperWindow.
#include "gui-single-instance.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTimer>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 20;
    const QString markers = QString::fromLocal8Bit(argv[1]);
    ViperGuiInstance instance;
    const auto result = instance.claim();
    if (result == ViperGuiInstance::StartResult::Forwarded) return 0;
    if (result != ViperGuiInstance::StartResult::Primary) return 21;

    instance.onActivation(&app, [markers](const QByteArray& token) {
        QFile activation(QDir(markers).filePath("activated"));
        if (activation.open(QIODevice::WriteOnly | QIODevice::Append)) {
            activation.write("restored\n");
            if (!token.isEmpty()) {
                activation.write("token=");
                activation.write(token);
                activation.write("\n");
            }
        }
    }, [markers](bool stop) {
        QFile request(QDir(markers).filePath("requested-quit"));
        if (!request.open(QIODevice::WriteOnly | QIODevice::Append)) return false;
        request.write(stop ? "stop\n" : "keep\n");
        return true; // Harness never actually quits or touches an audio backend.
    });
    QFile ready(QDir(markers).filePath("ready"));
    if (!ready.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 22;
    ready.write("primary\n");
    ready.close();
    // The test driver terminates this process explicitly. Give busy CI workers
    // ample time before the orphan guard exits a deliberately stalled harness.
    QTimer::singleShot(30000, &app, &QCoreApplication::quit);
    return app.exec();
}
