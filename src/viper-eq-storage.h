#pragma once
// Shared GUI/CLI storage policy for custom EQ preset names and precision.
// No DSP channel or user-interface dependencies.
#include <QSettings>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <vector>

namespace viper_eq_storage {

inline void migratePrecision(QSettings& settings) {
    if (settings.value("eq_precision_hundredths", false).toBool()) return;
    const auto convert = [&settings](const QString& key) {
        if (!settings.contains(key)) return;
        const auto fields = settings.value(key).toString().split(',');
        QStringList values;
        for (const auto& field : fields) {
            bool ok = false;
            const int prior = field.trimmed().toInt(&ok);
            if (!ok) return; // Leave malformed values untouched, as before.
            values << QString::number(std::clamp(prior * 10, -1200, 1200));
        }
        settings.setValue(key, values.join(','));
    };
    for (int count : {10, 15, 25, 31}) {
        convert("eq_levels_" + QString::number(count));
        convert("preset/eq_levels_" + QString::number(count));
    }
    settings.beginGroup("eq/user_presets");
    const QStringList names = settings.childGroups();
    settings.endGroup();
    for (const auto& name : names)
        convert("eq/user_presets/" + name + "/levels");
    settings.setValue("eq_precision_hundredths", true);
    settings.sync();
}

inline bool save(QSettings& settings, const QString& requested, int bands,
                 const std::vector<int>& values, QString& error) {
    const QString name = requested.trimmed();
    if (name.isEmpty() || name.size() > 70 || name.contains('/') ||
        name.contains('\\')) {
        error = "Use a preset name of 1 to 70 characters without slashes.";
        return false;
    }
    if (bands != 10 && bands != 15 && bands != 25 && bands != 31) {
        error = "EQ must use 10, 15, 25 or 31 bands.";
        return false;
    }
    if (static_cast<int>(values.size()) != bands ||
        std::any_of(values.begin(), values.end(),
                    [](int v) { return v < -1200 || v > 1200; })) {
        error = "EQ band levels are incomplete or out of range.";
        return false;
    }
    settings.sync();
    settings.beginGroup("eq/user_presets");
    const QStringList existing = settings.childGroups();
    settings.endGroup();
    for (const auto& other : existing) {
        if (other.compare(name, Qt::CaseInsensitive) == 0) {
            error = "A user EQ preset already has that name; nothing was overwritten.";
            return false;
        }
    }
    migratePrecision(settings);
    if (settings.status() != QSettings::NoError) {
        error = "Unable to migrate existing EQ preset precision.";
        return false;
    }
    QStringList csv;
    for (int v : values) csv << QString::number(v);
    const QString path = "eq/user_presets/" + name + "/";
    settings.setValue(path + "bands", bands);
    settings.setValue(path + "levels", csv.join(','));
    settings.setValue("eq/selected_preset", "user:" + name);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = "Could not save user EQ preset.";
        return false;
    }
    return true;
}

inline bool remove(QSettings& settings, const QString& requested, QString& error) {
    QString name = requested.trimmed();
    if (name.startsWith("user:", Qt::CaseInsensitive)) name = name.mid(5);
    settings.sync();
    settings.beginGroup("eq/user_presets");
    const QStringList existing = settings.childGroups();
    settings.endGroup();
    QString found;
    for (const auto& other : existing) {
        if (other.compare(name, Qt::CaseInsensitive) == 0) {
            found = other;
            break;
        }
    }
    if (found.isEmpty()) {
        error = "No user EQ preset has that name; built-in presets cannot be deleted.";
        return false;
    }
    settings.remove("eq/user_presets/" + found);
    const QString selected = settings.value("eq/selected_preset").toString();
    if (selected.compare("user:" + found, Qt::CaseInsensitive) == 0)
        settings.setValue("eq/selected_preset", "custom");
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = "Could not delete user EQ preset.";
        return false;
    }
    return true;
}

} // namespace viper_eq_storage
