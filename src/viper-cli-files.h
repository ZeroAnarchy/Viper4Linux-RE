#pragma once
// CLI counterpart to GUI correction-library selection and import.
// Validate before changing settings or IPC; never mutate the installed
// DSP/backend except by requesting the existing native reload worker.
#include "window/library-import.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace viperbass::cli_files {

inline bool validate(const QString& requested, bool ddc, bool import,
                     QString& error) {
    if (!import && requested == QStringLiteral("none")) return true;
    const QFileInfo file(requested);
    const QString suffix = file.suffix().toLower();
    if (!file.isFile() || !file.isReadable() ||
        (ddc ? suffix != QStringLiteral("vdc")
             : suffix != QStringLiteral("irs") && suffix != QStringLiteral("wav"))) {
        error = ddc ? QStringLiteral("Expected a readable .vdc profile, or none.")
                    : QStringLiteral("Expected a readable .irs/.wav response, or none.");
        return false;
    }
    return true;
}

inline bool resolve(const QString& requested, bool ddc, bool import,
                    QString& selected, QString& error) {
    if (!validate(requested, ddc, import, error)) return false;
    if (!import && requested == QStringLiteral("none")) {
        selected.clear();
        return true;
    }
    const QString source = QFileInfo(requested).canonicalFilePath();
    if (source.isEmpty()) {
        error = QStringLiteral("Source file could not be resolved.");
        return false;
    }
    if (!import) {
        selected = source;
        return true;
    }
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        (ddc ? QStringLiteral("/viper4linux/VDC")
             : QStringLiteral("/viper4linux/IRS"));
    if (!QDir().mkpath(directory)) {
        error = QStringLiteral("Could not create the correction library directory.");
        return false;
    }
    selected = viper_library_import::copyOrSelect(source, directory);
    if (selected.isEmpty()) {
        error = QStringLiteral("Could not import the selected correction file.");
        return false;
    }
    return true;
}

} // namespace viperbass::cli_files
