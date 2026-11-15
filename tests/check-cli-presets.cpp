// GUI-compatible complete named effect presets and root Reset CLI commands.
// Runs exclusively against a PID-scoped mock IPC server and disposable XDG
// settings/library locations; never touches a live PipeWire processor.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main
#include "mock/cli-eq-ack.h"
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static int execute(std::initializer_list<const char*> args) {
    std::vector<std::string> words = {"viper4linux-ctl"};
    for (const char* word : args) words.emplace_back(word);
    std::vector<char*> argv;
    for (auto& word : words) argv.push_back(word.data());
    return viper_ctl_entry(static_cast<int>(words.size()), argv.data());
}
int main() {
    QTemporaryDir config, data;
    assert(config.isValid() && data.isValid());
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    QSettings settings("ViPERforLinux", "ViPERClean");
    settings.setValue("ui/theme", "classic-v4a");
    settings.setValue("presets/legacy_migrated", true);
    settings.setValue("eq_precision_hundredths", true);
    settings.setValue("ddc_profile", "/example/equalizer.vdc");
    settings.setValue("convolver_kernel", "/example/room.irs");
    settings.setValue("eq/selected_preset", "custom");
    settings.setValue("precision/clarity_gain_milli", 4321);
    settings.sync();

    viperbass::Mapping server;
    assert(server.create_server());
    MockCliEqAcknowledger acknowledger(*server.get());
    auto& c = *server.get();
    c.enabled.store(1);
    c.bass_mode.store(2);
    c.strength_centi.store(237);
    c.clarity_enabled.store(1);
    c.clarity_gain_milli.store(2675);
    c.eq_enabled.store(1);
    c.eq_band_count.store(15);
    c.eq_levels_hundredths[1].store(175);
    c.mb_enabled.store(1);
    c.mb_band_count.store(5);
    c.mb_crossovers_hz[1].store(600);
    c.mb_bands[2].ratio_tenths.store(35);
    c.deq_enabled.store(1);
    c.deq_band_count.store(5);
    c.deq_bands[2].gain_hundredths.store(275);
    c.imager_high_crossover_hz.store(6000);
    c.fet_enabled.store(1);
    c.fet_threshold_db.store(-24);

    assert(execute({"--preset"}) == 0);
    assert(execute({"--preset", "list"}) == 0);
    assert(execute({"--preset", "help"}) == 0);
    assert(execute({"--preset", "--help"}) == 0);
    // Help is never a valid operation inside a changing effect sequence.
    // Preflight rejects either spelling BEFORE Bass is enabled or gain edited.
    c.enabled.store(0);
    c.strength_centi.store(237);
    c.bass_mode.store(2);
    assert(execute({"--bass", "on", "gain=3", "help"}) == 2);
    assert(c.enabled.load() == 0);
    assert(c.strength_centi.load() == 237);
    assert(execute({"--bass", "mode=1", "--help"}) == 2);
    assert(c.bass_mode.load() == 2);
    c.enabled.store(1); // Restore setup before snapshot/round-trip tests.
    assert(execute({"--preset", "help", "extra"}) == 2);
    assert(execute({"--preset", "save"}) == 2);
    assert(execute({"--preset", "save=Studio Warm"}) == 2);
    assert(execute({"--preset", "rename", "Studio Warm"}) == 2);
    assert(execute({"--preset", "save", "Studio Warm", "--on"}) == 2);
    assert(execute({"--preset-save", "Studio Warm"}) == 2); // Obsolete root spelling rejected.
    assert(execute({"--preset", "save", "Studio Warm"}) == 0);
    const auto saved = viper_presets::namedPresets();
    assert(saved.size() == 1);
    const QString originalPath = saved.front().path;
    assert(QFileInfo::exists(originalPath));
    {
        QSettings p(originalPath, QSettings::IniFormat);
        assert(p.value("format/version").toInt() == 1);
        assert(p.value("format/name").toString() == "Studio Warm");
        assert(p.value("effects/clarity_gain_tenths").toInt() == 26);
        assert(p.value("effects/imager_high_crossover_hundreds").toInt() == 60);
        assert(p.value("effects/mb_band_count").toInt() == 5);
        assert(p.value("effects/mb_b2_ratio_tenths").toInt() == 35);
        assert(p.value("effects/deq_b2_gain").toInt() == 275);
        assert(p.value("effects/eq_levels_15").toString().split(',').at(1) == "175");
        assert(!p.contains("ui/theme"));
        assert(!p.contains("precision/clarity_gain_milli"));
    }
    assert(execute({"--preset", "save", "studio warm"}) == 2); // Never overwrite silently.
    QString diagnostic;
    assert(!viperbass::cli_presets::save("Studio Warm", false, settings, c, diagnostic));
    // Test actionable syntax without pinning incidental placeholder wording.
    assert(diagnostic.contains("--preset overwrite"));
    diagnostic.clear();
    assert(!viperbass::cli_presets::save("Nonexistent Preset", true, settings, c, diagnostic));
    assert(diagnostic.contains("--preset save"));
    assert(viper_presets::namedPresets().size() == 1);
    assert(execute({"--preset", "save", "bad/name"}) == 2);
    assert(execute({"--preset", "rename", "Studio Warm", "Studio Night"}) == 0);
    assert(viper_presets::namedPresets().front().path == originalPath);
    assert(execute({"--preset", "rename", "Studio Night", "Studio Warm"}) == 0);

    c.bass_mode.store(1);
    assert(execute({"--preset", "overwrite", "Studio Warm"}) == 0);
    assert(viper_presets::namedPresets().size() == 1);
    assert(viper_presets::namedPresets().front().path == originalPath);
    c.bass_mode.store(0);
    c.clarity_gain_milli.store(1500);
    c.mb_band_count.store(3);
    c.mb_bands[2].ratio_tenths.store(20);
    c.eq_band_count.store(10);
    c.eq_levels_hundredths[1].store(0);
    c.fet_enabled.store(0);
    c.imager_high_crossover_hz.store(4000);
    assert(execute({"--preset", "load", "studio warm"}) == 0);
    assert(c.bass_mode.load() == 1);
    assert(c.clarity_gain_milli.load() == 2600); // GUI coarse slider projection.
    assert(c.mb_band_count.load() == 5);
    assert(c.mb_bands[2].ratio_tenths.load() == 35);
    assert(c.eq_band_count.load() == 15);
    assert(c.eq_levels_hundredths[1].load() == 175);
    assert(c.fet_enabled.load() == 1);
    assert(c.imager_high_crossover_hz.load() == 6000);
    settings.sync();
    assert(settings.value("imager_high_crossover_hz").toInt() == 6000);
    assert(settings.value("ui/theme").toString() == "classic-v4a");
    assert(!settings.contains("precision/clarity_gain_milli"));
    assert(settings.value("ddc_profile").toString() == "/example/equalizer.vdc");

    // Reject a structurally correct but semantically corrupt file before
    // modifying either the user configuration or any live DSP field.
    const int before = c.bass_mode.load();
    const QString savedMode = settings.value("bass_mode").toString();
    {
        QSettings p(originalPath, QSettings::IniFormat);
        p.setValue("effects/mb_band_count", 4);
        p.sync();
    }
    assert(execute({"--preset", "load", "Studio Warm"}) == 2);
    assert(c.bass_mode.load() == before);
    settings.sync();
    assert(settings.value("bass_mode").toString() == savedMode);

    // Older GUI presets may omit newer effects. Those fields use the same
    // default values as GUI restore, rather than inheriting live DSP edits.
    const QString legacyPath = data.filePath("viper4linux/presets/legacy.ini");
    {
        QSettings old(legacyPath, QSettings::IniFormat);
        old.setValue("format/version", 1);
        old.setValue("format/name", "Old GUI");
        old.setValue("effects/master_enabled", true);
        old.setValue("effects/enabled", true);
        old.setValue("effects/limiter_output_percent", 89);
        old.sync();
    }
    assert(execute({"--preset", "load", "Old GUI"}) == 0);
    assert(c.limiter_output_percent.load() == 89);
    assert(c.fet_enabled.load() == 0);
    assert(c.eq_band_count.load() == 10);
    assert(c.mb_band_count.load() == 3);
    assert(c.deq_bands[0].frequency_hz.load() == 100);

    // Reset is a separate root command, not a hidden preset operation.
    c.master_enabled.store(0);
    c.enabled.store(1);
    c.eq_band_count.store(31);
    c.eq_levels_hundredths[4].store(1200);
    c.fet_enabled.store(1);
    c.deq_enabled.store(1);
    c.ddc_enabled.store(1);
    c.convolver_enabled.store(1);
    settings.setValue("precision/strength_centi", 222);
    settings.setValue("ui/theme", "classic-v4a");
    settings.setValue("ddc_profile", "/example/equalizer.vdc");
    settings.setValue("convolver_kernel", "/example/room.irs");
    settings.sync();
    assert(execute({"--reset-effects"}) == 0);
    assert(c.master_enabled.load() == 1);
    assert(c.enabled.load() == 0);
    assert(c.eq_enabled.load() == 0);
    assert(c.eq_band_count.load() == 10);
    for (int i = 0; i < 31; ++i) assert(c.eq_levels_hundredths[i].load() == 0);
    assert(c.fet_enabled.load() == 0);
    assert(c.mb_enabled.load() == 0 && c.mb_band_count.load() == 3);
    assert(c.deq_enabled.load() == 0 && c.deq_band_count.load() == 3);
    assert(c.deq_bands[0].frequency_hz.load() == 100);
    assert(c.ddc_enabled.load() == 0);
    assert(c.convolver_enabled.load() == 0);
    assert(c.limiter_threshold_permille.load() == 500);
    assert(c.imager_high_crossover_hz.load() == 4000);
    settings.sync();
    assert(settings.value("eq/selected_preset").toString() == "custom");
    assert(settings.value("ddc_profile").toString() == "/example/equalizer.vdc");
    assert(settings.value("convolver_kernel").toString() == "/example/room.irs");
    assert(settings.value("ui/theme").toString() == "classic-v4a");
    assert(!settings.contains("precision/strength_centi"));
    assert(QFileInfo::exists(originalPath));
    assert(viper_presets::namedPresets().size() == 2);
    assert(execute({"--reset-effects", "--on"}) == 2); // Root command only.

    acknowledger.stop();
    server.stop_server();
    assert(execute({"--preset"}) == 0);
    assert(execute({"--preset", "list"}) == 0);
    assert(execute({"--preset", "help"}) == 0); // Offline help is read-only.
    assert(execute({"--preset", "delete", "Studio Warm"}) == 0); // No backend needed.
    assert(execute({"--preset", "load", "Old GUI"}) == 1); // A live channel IS needed.
    assert(viper_presets::namedPresets().size() == 1);
    std::cout << "PASS: named effect presets and independent root reset\n";
}
