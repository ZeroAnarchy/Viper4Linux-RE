#pragma once
// Complete named-effect-preset bridge for the short-lived CLI.
// Uses the GUI's versioned .ini schema and exact shared-memory control fields.
// No GUI widgets, PipeWire callbacks, or real-time filesystem I/O.
#include "viper-control.h"
#include "viper-named-presets.h"
#include "viper-eq-storage.h"
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <ostream>
#include <vector>

namespace viperbass::cli_presets {
using Snapshot = QMap<QString, QVariant>;
struct Binding {
    QString key;
    std::atomic<std::int32_t>* field;
    int minimum;
    int maximum;
    int scale;
    bool boolean;
};
inline std::vector<Binding> bindings(Controls& c) {
    std::vector<Binding> result;
    result.reserve(196);
    const auto add = [&](const char* key, std::atomic<std::int32_t>& value,
                         int lo, int hi, int scale, bool boolean) {
        result.push_back({QString::fromLatin1(key), &value, lo, hi, scale, boolean});
    };
    add("master_enabled", c.master_enabled, 0, 1, 1, true);
    add("enabled", c.enabled, 0, 1, 1, true);
    add("bass_mode", c.bass_mode, 0, 2, 1, false);
    add("strength_tenths", c.strength_centi, 5, 100, 10, false);
    add("frequency", c.frequency_hz, 15, 150, 1, false);
    add("bass_mono_enabled", c.bass_mono_enabled, 0, 1, 1, true);
    add("bass_mono_mode", c.bass_mono_mode, 0, 2, 1, false);
    add("bass_mono_strength_centi", c.bass_mono_strength_centi, 50, 1000, 1, false);
    add("bass_mono_frequency_hz", c.bass_mono_frequency_hz, 15, 150, 1, false);
    add("psycho_enabled", c.psycho_enabled, 0, 1, 1, true);
    add("psycho_cutoff_hz", c.psycho_cutoff_hz, 60, 150, 1, false);
    add("psycho_intensity_percent", c.psycho_intensity_percent, 0, 100, 1, false);
    add("psycho_order", c.psycho_order, 2, 5, 1, false);
    add("psycho_original_percent", c.psycho_original_percent, 0, 100, 1, false);
    add("dynamic_enabled", c.dynamic_enabled, 0, 1, 1, true);
    add("dynamic_x_low", c.dynamic_x_low, 0, 2400, 1, false);
    add("dynamic_x_high", c.dynamic_x_high, 0, 12000, 1, false);
    add("dynamic_y_low", c.dynamic_y_low, 0, 200, 1, false);
    add("dynamic_y_high", c.dynamic_y_high, 0, 300, 1, false);
    add("dynamic_side_low_percent", c.dynamic_side_low_percent, 0, 100, 1, false);
    add("dynamic_side_high_percent", c.dynamic_side_high_percent, 0, 100, 1, false);
    add("dynamic_strength_percent", c.dynamic_strength_percent, 0, 100, 1, false);
    add("eq_enabled", c.eq_enabled, 0, 1, 1, true);
    add("eq_band_count", c.eq_band_count, 10, 31, 1, false);
    add("clarity_enabled", c.clarity_enabled, 0, 1, 1, true);
    add("clarity_mode", c.clarity_mode, 0, 2, 1, false);
    add("clarity_gain_tenths", c.clarity_gain_milli, 0, 45, 100, false);
    add("spectrum_enabled", c.spectrum_enabled, 0, 1, 1, true);
    add("spectrum_frequency_hundreds", c.spectrum_frequency_hz, 22, 82, 100, false);
    add("spectrum_exciter_tenths", c.spectrum_exciter_milli, 0, 60, 100, false);
    add("tube_enabled", c.tube_enabled, 0, 1, 1, true);
    add("tube_model", c.tube_model, 0, 2, 1, false);
    add("tube_drive_percent", c.tube_drive_percent, 0, 100, 1, false);
    add("tube_mix_percent", c.tube_mix_percent, 0, 100, 1, false);
    add("analogx_enabled", c.analogx_enabled, 0, 1, 1, true);
    add("analogx_mode", c.analogx_mode, 0, 2, 1, false);
    add("crossfeed_enabled", c.crossfeed_enabled, 0, 1, 1, true);
    add("crossfeed_preset", c.crossfeed_preset, 0, 2, 1, false);
    add("fet_enabled", c.fet_enabled, 0, 1, 1, true);
    add("fet_threshold_db", c.fet_threshold_db, -48, 0, 1, false);
    add("fet_ratio_tenths", c.fet_ratio_tenths, 10, 100, 1, false);
    add("fet_knee_db", c.fet_knee_db, 0, 12, 1, false);
    add("fet_makeup_db", c.fet_makeup_db, 0, 24, 1, false);
    add("fet_attack_ms", c.fet_attack_ms, 1, 100, 1, false);
    add("fet_release_ms", c.fet_release_ms, 5, 500, 1, false);
    add("mb_enabled", c.mb_enabled, 0, 1, 1, true);
    add("mb_band_count", c.mb_band_count, 3, 5, 1, false);
    add("ddc_enabled", c.ddc_enabled, 0, 1, 1, true);
    add("speaker_enabled", c.speaker_enabled, 0, 1, 1, true);
    add("convolver_enabled", c.convolver_enabled, 0, 1, 1, true);
    add("convolver_cross_percent", c.convolver_cross_percent, 0, 100, 1, false);
    add("imager_enabled", c.imager_enabled, 0, 1, 1, true);
    add("imager_low_width_percent", c.imager_low_width_percent, 0, 200, 1, false);
    add("imager_mid_width_percent", c.imager_mid_width_percent, 0, 200, 1, false);
    add("imager_high_width_percent", c.imager_high_width_percent, 0, 200, 1, false);
    add("imager_low_crossover_hz", c.imager_low_crossover_hz, 80, 400, 1, false);
    add("imager_high_crossover_hundreds", c.imager_high_crossover_hz, 20, 80, 100, false);
    add("headphone_enabled", c.headphone_enabled, 0, 1, 1, true);
    add("headphone_quality", c.headphone_quality, 0, 4, 1, false);
    add("diff_enabled", c.diff_enabled, 0, 1, 1, true);
    add("diff_delay_ms", c.diff_delay_ms, 1, 20, 1, false);
    add("diff_reverse", c.diff_reverse, 0, 1, 1, true);
    add("diff_mix_percent", c.diff_mix_percent, 0, 100, 1, false);
    add("diff_lp_hundreds", c.diff_lp_hz, 0, 200, 100, false);
    add("field_enabled", c.field_enabled, 0, 1, 1, true);
    add("field_widen_tenths", c.field_widen_tenths, 0, 80, 1, false);
    add("field_mid_hundredths", c.field_mid_hundredths, 100, 200, 1, false);
    add("field_depth_step", c.field_depth, 0, 16, 1, false);
    add("reverb_enabled", c.reverb_enabled, 0, 1, 1, true);
    add("reverb_room_percent", c.reverb_room_percent, 0, 100, 1, false);
    add("reverb_width_percent", c.reverb_width_percent, 0, 100, 1, false);
    add("reverb_damp_percent", c.reverb_damp_percent, 0, 100, 1, false);
    add("reverb_wet_percent", c.reverb_wet_percent, 0, 100, 1, false);
    add("reverb_dry_percent", c.reverb_dry_percent, 0, 100, 1, false);
    add("limiter_threshold_permille", c.limiter_threshold_permille, 300, 1000, 1, false);
    add("limiter_output_percent", c.limiter_output_percent, 1, 200, 1, false);
    add("limiter_pan_percent", c.limiter_pan_percent, -100, 100, 1, false);
    add("agc_enabled", c.agc_enabled, 0, 1, 1, true);
    add("agc_strength_centi", c.agc_strength_centi, 50, 300, 1, false);
    add("agc_max_gain_centi", c.agc_max_gain_centi, 100, 1000, 1, false);
    add("agc_threshold_permille", c.agc_threshold_permille, 300, 1000, 1, false);
    add("lufs_enabled", c.lufs_enabled, 0, 1, 1, true);
    add("lufs_target_tenths", c.lufs_target_tenths, -240, -80, 1, false);
    add("lufs_max_gain_tenths", c.lufs_max_gain_tenths, 0, 120, 1, false);
    add("lufs_speed", c.lufs_speed, 0, 2, 1, false);
    add("deq_enabled", c.deq_enabled, 0, 1, 1, true);
    add("deq_band_count", c.deq_band_count, 1, 10, 1, false);
    add("mb_x0_hz", c.mb_crossovers_hz[0], 80, 15000, 1, false);
    add("mb_x1_hz", c.mb_crossovers_hz[1], 80, 15000, 1, false);
    add("mb_x2_hz", c.mb_crossovers_hz[2], 80, 15000, 1, false);
    add("mb_x3_hz", c.mb_crossovers_hz[3], 80, 15000, 1, false);
    add("mb_b0_enabled", c.mb_bands[0].enabled, 0, 1, 1, true);
    add("mb_b0_threshold_db", c.mb_bands[0].threshold_db, -48, 0, 1, false);
    add("mb_b0_ratio_tenths", c.mb_bands[0].ratio_tenths, 10, 100, 1, false);
    add("mb_b0_knee_db", c.mb_bands[0].knee_db, 0, 12, 1, false);
    add("mb_b0_makeup_db", c.mb_bands[0].makeup_db, 0, 24, 1, false);
    add("mb_b0_attack_ms", c.mb_bands[0].attack_ms, 1, 100, 1, false);
    add("mb_b0_release_ms", c.mb_bands[0].release_ms, 5, 500, 1, false);
    add("mb_b1_enabled", c.mb_bands[1].enabled, 0, 1, 1, true);
    add("mb_b1_threshold_db", c.mb_bands[1].threshold_db, -48, 0, 1, false);
    add("mb_b1_ratio_tenths", c.mb_bands[1].ratio_tenths, 10, 100, 1, false);
    add("mb_b1_knee_db", c.mb_bands[1].knee_db, 0, 12, 1, false);
    add("mb_b1_makeup_db", c.mb_bands[1].makeup_db, 0, 24, 1, false);
    add("mb_b1_attack_ms", c.mb_bands[1].attack_ms, 1, 100, 1, false);
    add("mb_b1_release_ms", c.mb_bands[1].release_ms, 5, 500, 1, false);
    add("mb_b2_enabled", c.mb_bands[2].enabled, 0, 1, 1, true);
    add("mb_b2_threshold_db", c.mb_bands[2].threshold_db, -48, 0, 1, false);
    add("mb_b2_ratio_tenths", c.mb_bands[2].ratio_tenths, 10, 100, 1, false);
    add("mb_b2_knee_db", c.mb_bands[2].knee_db, 0, 12, 1, false);
    add("mb_b2_makeup_db", c.mb_bands[2].makeup_db, 0, 24, 1, false);
    add("mb_b2_attack_ms", c.mb_bands[2].attack_ms, 1, 100, 1, false);
    add("mb_b2_release_ms", c.mb_bands[2].release_ms, 5, 500, 1, false);
    add("mb_b3_enabled", c.mb_bands[3].enabled, 0, 1, 1, true);
    add("mb_b3_threshold_db", c.mb_bands[3].threshold_db, -48, 0, 1, false);
    add("mb_b3_ratio_tenths", c.mb_bands[3].ratio_tenths, 10, 100, 1, false);
    add("mb_b3_knee_db", c.mb_bands[3].knee_db, 0, 12, 1, false);
    add("mb_b3_makeup_db", c.mb_bands[3].makeup_db, 0, 24, 1, false);
    add("mb_b3_attack_ms", c.mb_bands[3].attack_ms, 1, 100, 1, false);
    add("mb_b3_release_ms", c.mb_bands[3].release_ms, 5, 500, 1, false);
    add("mb_b4_enabled", c.mb_bands[4].enabled, 0, 1, 1, true);
    add("mb_b4_threshold_db", c.mb_bands[4].threshold_db, -48, 0, 1, false);
    add("mb_b4_ratio_tenths", c.mb_bands[4].ratio_tenths, 10, 100, 1, false);
    add("mb_b4_knee_db", c.mb_bands[4].knee_db, 0, 12, 1, false);
    add("mb_b4_makeup_db", c.mb_bands[4].makeup_db, 0, 24, 1, false);
    add("mb_b4_attack_ms", c.mb_bands[4].attack_ms, 1, 100, 1, false);
    add("mb_b4_release_ms", c.mb_bands[4].release_ms, 5, 500, 1, false);
    add("deq_b0_freq", c.deq_bands[0].frequency_hz, 20, 20000, 1, false);
    add("deq_b0_q", c.deq_bands[0].q_hundredths, 50, 800, 1, false);
    add("deq_b0_gain", c.deq_bands[0].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b0_threshold", c.deq_bands[0].threshold_tenths, -800, 0, 1, false);
    add("deq_b0_attack", c.deq_bands[0].attack_ms, 1, 100, 1, false);
    add("deq_b0_release", c.deq_bands[0].release_ms, 10, 500, 1, false);
    add("deq_b0_type", c.deq_bands[0].filter_type, 0, 2, 1, false);
    add("deq_b1_freq", c.deq_bands[1].frequency_hz, 20, 20000, 1, false);
    add("deq_b1_q", c.deq_bands[1].q_hundredths, 50, 800, 1, false);
    add("deq_b1_gain", c.deq_bands[1].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b1_threshold", c.deq_bands[1].threshold_tenths, -800, 0, 1, false);
    add("deq_b1_attack", c.deq_bands[1].attack_ms, 1, 100, 1, false);
    add("deq_b1_release", c.deq_bands[1].release_ms, 10, 500, 1, false);
    add("deq_b1_type", c.deq_bands[1].filter_type, 0, 2, 1, false);
    add("deq_b2_freq", c.deq_bands[2].frequency_hz, 20, 20000, 1, false);
    add("deq_b2_q", c.deq_bands[2].q_hundredths, 50, 800, 1, false);
    add("deq_b2_gain", c.deq_bands[2].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b2_threshold", c.deq_bands[2].threshold_tenths, -800, 0, 1, false);
    add("deq_b2_attack", c.deq_bands[2].attack_ms, 1, 100, 1, false);
    add("deq_b2_release", c.deq_bands[2].release_ms, 10, 500, 1, false);
    add("deq_b2_type", c.deq_bands[2].filter_type, 0, 2, 1, false);
    add("deq_b3_freq", c.deq_bands[3].frequency_hz, 20, 20000, 1, false);
    add("deq_b3_q", c.deq_bands[3].q_hundredths, 50, 800, 1, false);
    add("deq_b3_gain", c.deq_bands[3].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b3_threshold", c.deq_bands[3].threshold_tenths, -800, 0, 1, false);
    add("deq_b3_attack", c.deq_bands[3].attack_ms, 1, 100, 1, false);
    add("deq_b3_release", c.deq_bands[3].release_ms, 10, 500, 1, false);
    add("deq_b3_type", c.deq_bands[3].filter_type, 0, 2, 1, false);
    add("deq_b4_freq", c.deq_bands[4].frequency_hz, 20, 20000, 1, false);
    add("deq_b4_q", c.deq_bands[4].q_hundredths, 50, 800, 1, false);
    add("deq_b4_gain", c.deq_bands[4].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b4_threshold", c.deq_bands[4].threshold_tenths, -800, 0, 1, false);
    add("deq_b4_attack", c.deq_bands[4].attack_ms, 1, 100, 1, false);
    add("deq_b4_release", c.deq_bands[4].release_ms, 10, 500, 1, false);
    add("deq_b4_type", c.deq_bands[4].filter_type, 0, 2, 1, false);
    add("deq_b5_freq", c.deq_bands[5].frequency_hz, 20, 20000, 1, false);
    add("deq_b5_q", c.deq_bands[5].q_hundredths, 50, 800, 1, false);
    add("deq_b5_gain", c.deq_bands[5].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b5_threshold", c.deq_bands[5].threshold_tenths, -800, 0, 1, false);
    add("deq_b5_attack", c.deq_bands[5].attack_ms, 1, 100, 1, false);
    add("deq_b5_release", c.deq_bands[5].release_ms, 10, 500, 1, false);
    add("deq_b5_type", c.deq_bands[5].filter_type, 0, 2, 1, false);
    add("deq_b6_freq", c.deq_bands[6].frequency_hz, 20, 20000, 1, false);
    add("deq_b6_q", c.deq_bands[6].q_hundredths, 50, 800, 1, false);
    add("deq_b6_gain", c.deq_bands[6].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b6_threshold", c.deq_bands[6].threshold_tenths, -800, 0, 1, false);
    add("deq_b6_attack", c.deq_bands[6].attack_ms, 1, 100, 1, false);
    add("deq_b6_release", c.deq_bands[6].release_ms, 10, 500, 1, false);
    add("deq_b6_type", c.deq_bands[6].filter_type, 0, 2, 1, false);
    add("deq_b7_freq", c.deq_bands[7].frequency_hz, 20, 20000, 1, false);
    add("deq_b7_q", c.deq_bands[7].q_hundredths, 50, 800, 1, false);
    add("deq_b7_gain", c.deq_bands[7].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b7_threshold", c.deq_bands[7].threshold_tenths, -800, 0, 1, false);
    add("deq_b7_attack", c.deq_bands[7].attack_ms, 1, 100, 1, false);
    add("deq_b7_release", c.deq_bands[7].release_ms, 10, 500, 1, false);
    add("deq_b7_type", c.deq_bands[7].filter_type, 0, 2, 1, false);
    add("deq_b8_freq", c.deq_bands[8].frequency_hz, 20, 20000, 1, false);
    add("deq_b8_q", c.deq_bands[8].q_hundredths, 50, 800, 1, false);
    add("deq_b8_gain", c.deq_bands[8].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b8_threshold", c.deq_bands[8].threshold_tenths, -800, 0, 1, false);
    add("deq_b8_attack", c.deq_bands[8].attack_ms, 1, 100, 1, false);
    add("deq_b8_release", c.deq_bands[8].release_ms, 10, 500, 1, false);
    add("deq_b8_type", c.deq_bands[8].filter_type, 0, 2, 1, false);
    add("deq_b9_freq", c.deq_bands[9].frequency_hz, 20, 20000, 1, false);
    add("deq_b9_q", c.deq_bands[9].q_hundredths, 50, 800, 1, false);
    add("deq_b9_gain", c.deq_bands[9].gain_hundredths, -1200, 1200, 1, false);
    add("deq_b9_threshold", c.deq_bands[9].threshold_tenths, -800, 0, 1, false);
    add("deq_b9_attack", c.deq_bands[9].attack_ms, 1, 100, 1, false);
    add("deq_b9_release", c.deq_bands[9].release_ms, 10, 500, 1, false);
    add("deq_b9_type", c.deq_bands[9].filter_type, 0, 2, 1, false);
    return result;
}

inline QString flatEq(int count) {
    QStringList fields;
    for (int i = 0; i < count; ++i) fields << QStringLiteral("0");
    return fields.join(',');
}
inline bool normalizeEq(const QString& input, int count, QString& output) {
    const auto fields = input.split(',');
    if (fields.size() != count) return false;
    QStringList fixed;
    for (const QString& field : fields) {
        bool ok = false;
        const int level = field.trimmed().toInt(&ok);
        if (!ok || level < -1200 || level > 1200) return false;
        fixed << QString::number(level);
    }
    output = fixed.join(',');
    return true;
}
inline void setGuiDefaults(Controls& c) {
    // Match ViperWindow::resetEffectsWithConfirmation(), not incidental IPC
    // construction values. The GUI gives inactive Dynamic EQ bands the
    // ten-band starting table, while the first three use 100/1000/8000 Hz.
    c.enabled.store(0);
    c.dynamic_x_low.store(120);
    constexpr int freq[] = {100, 1000, 8000, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < 10; ++i) c.deq_bands[i].frequency_hz.store(freq[i]);
}
inline Snapshot capture(QSettings& settings, Controls& live, bool factory = false) {
    Controls defaults;
    if (factory) setGuiDefaults(defaults);
    Controls& c = factory ? defaults : live;
    Snapshot values;
    for (const auto& b : bindings(c)) {
        const int native = b.field->load(std::memory_order_relaxed);
        // Match the GUI slider projection: legacy preset values truncate
        // scaled precision rather than rounding it. Depth is the one
        // exception; its slider deliberately rounds to the nearest step.
        const int projected = b.key == QStringLiteral("field_depth_step")
            ? (native == 0 ? 0 : std::clamp(
                  static_cast<int>(std::lround((native - 150) / 50.0)), 0, 16))
            : b.key == QStringLiteral("bass_mono_strength_centi")
                ? (native / 10) * 10 : native / b.scale;
        values.insert(b.key, b.boolean ? QVariant(projected != 0)
                                       : QVariant(std::clamp(projected, b.minimum, b.maximum)));
    }
    for (int i = 0; i < 10; ++i) {
        const QString key = QString("deq_b%1_user_modified").arg(i);
        values.insert(key, !factory && settings.value(key, false).toBool());
    }
    const int current = c.eq_band_count.load();
    for (int count : {10, 15, 25, 31}) {
        const QString key = "eq_levels_" + QString::number(count);
        QString csv = flatEq(count);
        if (!factory) {
            if (current == count) {
                QStringList fields;
                for (int i = 0; i < count; ++i)
                    fields << QString::number(std::clamp(
                        c.eq_levels_hundredths[i].load(), -1200, 1200));
                csv = fields.join(',');
            } else {
                QString normalized;
                if (normalizeEq(settings.value(key).toString(), count, normalized))
                    csv = normalized;
            }
        }
        values.insert(key, csv);
    }
    values.insert("eq_selected_preset", factory ? QStringLiteral("custom")
        : settings.value("eq/selected_preset", "custom").toString());
    values.insert("ddc_profile", factory ? QString()
        : settings.value("ddc_profile").toString());
    values.insert("convolver_kernel", factory ? QString()
        : settings.value("convolver_kernel").toString());
    return values;
}
inline bool readPreset(const QString& path, Snapshot& values, QString& error) {
    QSettings preset(path, QSettings::IniFormat);
    if (preset.status() != QSettings::NoError ||
        preset.value("format/version").toInt() != 1 ||
        !preset.contains("effects/master_enabled") ||
        !preset.contains("effects/enabled") ||
        !preset.contains("effects/limiter_output_percent")) {
        error = QStringLiteral("Not a valid version-1 ViPER effect preset.");
        return false;
    }
    QSettings settings("ViPERforLinux", "ViPERClean");
    Controls defaults;
    values = capture(settings, defaults, true);
    const auto schema = bindings(defaults);
    QMap<QString, Binding> lookup;
    for (const auto& b : schema) lookup.insert(b.key, b);
    for (const QString& fullKey : preset.allKeys()) {
        if (!fullKey.startsWith("effects/")) continue;
        const QString key = fullKey.mid(8);
        const QString literal = preset.value(fullKey).toString().trimmed();
        if (lookup.contains(key)) {
            const auto& b = lookup[key];
            bool ok = false;
            int value = 0;
            if (b.boolean && (literal == "true" || literal == "false"))
                value = literal == "true" ? 1 : 0, ok = true;
            else
                value = literal.toInt(&ok);
            if (!ok || value < b.minimum || value > b.maximum ||
                (key == "eq_band_count" && value != 10 && value != 15 &&
                    value != 25 && value != 31) ||
                (key == "mb_band_count" && value != 3 && value != 5)) {
                error = "Invalid or out-of-range preset field: " + key;
                return false;
            }
            values.insert(key, b.boolean ? QVariant(value != 0) : QVariant(value));
        } else if (key == "eq_selected_preset") {
            if (literal.size() > 256) { error = "Invalid EQ preset label."; return false; }
            values.insert(key, literal.isEmpty() ? QStringLiteral("custom") : literal);
        } else if (key == "ddc_profile" || key == "convolver_kernel") {
            if (literal.size() > 4096) { error = "Correction path is too long."; return false; }
            values.insert(key, literal);
        } else if (key.startsWith("eq_levels_")) {
            const QString suffix = key.mid(QStringLiteral("eq_levels_").size());
            bool ok = false;
            const int count = suffix.toInt(&ok);
            if (!ok || (count != 10 && count != 15 && count != 25 && count != 31)) {
                error = "Invalid EQ layout in preset.";
                return false;
            }
            QString normalized;
            if (!normalizeEq(literal, count, normalized)) {
                error = "Invalid EQ levels in preset: " + key;
                return false;
            }
            values.insert(key, normalized);
        } else if (key.startsWith("deq_b") && key.endsWith("_user_modified")) {
            const QString index = key.mid(5, key.size() - 5 - 14);
            bool ok = false;
            const int i = index.toInt(&ok);
            if (!ok || i < 0 || i > 9 ||
                (literal != "true" && literal != "false" &&
                 literal != "1" && literal != "0")) {
                error = "Invalid Dynamic EQ band modification marker.";
                return false;
            }
            values.insert(key, literal == "true" || literal == "1");
        }
        // Ignore unknown fields for forward-compatible version-1 presets.
    }
    return true;
}
inline bool apply(QSettings& settings, Controls& c, const Snapshot& values,
                  QString& error) {
    // Caller has validated the entire snapshot. Save all settings first; do
    // not partially change the live DSP if the settings store is unwritable.
    settings.sync();
    for (const auto& b : bindings(c)) {
        // The GUI stores this slider as hundreds for named presets, but
        // the active settings key holds the native crossover frequency.
        if (b.key == QStringLiteral("imager_high_crossover_hundreds"))
            settings.setValue("imager_high_crossover_hz", values.value(b.key).toInt() * 100);
        else
            settings.setValue(b.key, values.value(b.key));
    }
    const int count = values.value("eq_band_count").toInt();
    for (int bands : {10, 15, 25, 31})
        settings.setValue("eq_levels_" + QString::number(bands),
                          values.value("eq_levels_" + QString::number(bands)));
    settings.setValue("eq/selected_preset", values.value("eq_selected_preset"));
    settings.setValue("eq_precision_hundredths", true);
    for (int i = 0; i < 10; ++i) {
        const QString key = QString("deq_b%1_user_modified").arg(i);
        settings.setValue(key, values.value(key));
    }
    for (const char* path : {"ddc_profile", "convolver_kernel"}) {
        if (values.value(path).toString().isEmpty()) settings.remove(path);
        else settings.setValue(path, values.value(path));
    }
    // Full GUI preset loads and Reset replace the legacy coarse projection
    // and clear exact native overrides, including for the next backend start.
    settings.remove("precision");
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = "Failed to persist the complete configuration; DSP unchanged.";
        return false;
    }

    // Publish one complete logical change, with worker notifications emitted
    // only after all controls and all selected file paths have been updated.
    for (const auto& b : bindings(c)) {
        const int logical = values.value(b.key).toInt();
        const int native = b.key == QStringLiteral("field_depth_step")
            ? (logical == 0 ? 0 : 150 + logical * 50)
            : logical * b.scale;
        b.field->store(native, std::memory_order_relaxed);
    }
    QStringList levels = values.value("eq_levels_" + QString::number(count)).toString().split(',');
    for (int i = 0; i < 31; ++i)
        c.eq_levels_hundredths[i].store(i < count ? levels.at(i).toInt() : 0,
                                       std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    c.tube_request_serial.fetch_add(1, std::memory_order_release);
    c.analogx_request_serial.fetch_add(1, std::memory_order_release);
    c.crossfeed_request_serial.fetch_add(1, std::memory_order_release);
    c.fet_request_serial.fetch_add(1, std::memory_order_release);
    c.mb_request_serial.fetch_add(1, std::memory_order_release);
    c.ddc_request_serial.fetch_add(1, std::memory_order_release);
    c.speaker_request_serial.fetch_add(1, std::memory_order_release);
    c.imager_request_serial.fetch_add(1, std::memory_order_release);
    c.deq_request_serial.fetch_add(1, std::memory_order_release);
    c.convolver_request_serial.fetch_add(1, std::memory_order_release);
    c.eq_request_serial.fetch_add(1, std::memory_order_release);
    return true;
}
inline bool resolveByName(const QString& requested, viper_presets::NamedPreset& found,
                          QString& error) {
    const QString name = requested.trimmed();
    if (!viper_presets::acceptablePresetName(name)) {
        error = "Preset names must be 1-70 printable characters without slashes.";
        return false;
    }
    for (const auto& item : viper_presets::namedPresets()) {
        if (item.name.compare(name, Qt::CaseInsensitive) != 0) continue;
        found = item;
        return true;
    }
    error = "No named effect preset matches: " + name;
    return false;
}
inline bool save(const QString& requested, bool overwrite, QSettings& settings,
                 Controls& c, QString& error) {
    const QString name = requested.trimmed();
    if (!viper_presets::acceptablePresetName(name)) {
        error = "Preset names must be 1-70 printable characters without slashes.";
        return false;
    }
    // A first-time CLI user must not lose the GUI's single-slot legacy preset.
    if (!viper_presets::migrateLegacyPreset(settings, &error)) return false;
    viper_presets::NamedPreset existing;
    QString lookupError;
    const bool exists = resolveByName(name, existing, lookupError);
    if (exists != overwrite) {
        error = exists ? "Preset already exists; use --preset overwrite NAME explicitly."
                       : "Preset does not exist; use --preset save NAME to create it.";
        return false;
    }
    const QString path = exists ? existing.path : QDir(viper_presets::presetLibraryDirectory())
        .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ini");
    viper_eq_storage::migratePrecision(settings);
    QTemporaryDir temporary;
    if (!temporary.isValid()) { error = "Could not create temporary preset staging."; return false; }
    QSettings staging(temporary.filePath("snapshot.ini"), QSettings::IniFormat);
    const auto state = capture(settings, c);
    for (auto it = state.constBegin(); it != state.constEnd(); ++it)
        staging.setValue("preset/" + it.key(), it.value());
    staging.sync();
    if (staging.status() != QSettings::NoError) {
        error = "Could not stage complete configuration.";
        return false;
    }
    // GUI native writer keeps the UUID filename, version 1 and effect keys.
    return viper_presets::writeNamedPreset(staging, exists ? existing.name : name,
                                          path, &error);
}
inline bool rename(const QString& from, const QString& to, QString& error) {
    viper_presets::NamedPreset original;
    if (!resolveByName(from, original, error)) return false;
    const QString updated = to.trimmed();
    if (!viper_presets::acceptablePresetName(updated)) {
        error = "Preset names must be 1-70 printable characters without slashes.";
        return false;
    }
    for (const auto& item : viper_presets::namedPresets()) {
        if (item.path != original.path &&
            item.name.compare(updated, Qt::CaseInsensitive) == 0) {
            error = "A different preset already has that name.";
            return false;
        }
    }
    QSettings file(original.path, QSettings::IniFormat);
    file.setValue("format/name", updated);
    file.sync();
    if (file.status() != QSettings::NoError) {
        error = "Could not update preset name.";
        return false;
    }
    return true;
}
inline bool remove(const QString& requested, QString& error) {
    viper_presets::NamedPreset found;
    if (!resolveByName(requested, found, error)) return false;
    if (!QFile::remove(found.path)) {
        error = "Could not delete named preset.";
        return false;
    }
    return true;
}
inline void list(std::ostream& out) {
    const auto entries = viper_presets::namedPresets();
    out << "Named effect presets:\n";
    if (entries.empty()) out << "  (none)\n";
    for (const auto& item : entries)
        out << "  " << item.name.toStdString() << '\n';
}
} // namespace viperbass::cli_presets
