// Semantic green connection/activity and yellow bypass warning, without
// dependence on GUI source text or a live backend.
#include "gui-audio-status.h"
#include <QColor>
#include <QCoreApplication>
#include <cmath>
#include <cassert>
#include <iostream>

static double linear(int channel) {
    const double s = double(channel) / 255.0;
    return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}
static double contrast(const QColor& color, bool dark) {
    const double l = 0.2126 * linear(color.red()) +
                     0.7152 * linear(color.green()) +
                     0.0722 * linear(color.blue());
    return dark ? (l + 0.05) / 0.05 : 1.05 / (l + 0.05);
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using viperbass::BackendAudioActivity;
    const auto states = {BackendAudioActivity::Active,
                             BackendAudioActivity::NoSignal,
                             BackendAudioActivity::Idle,
                             BackendAudioActivity::Checking};
    for (bool dark : {false, true}) {
        for (const auto state : states) {
            const auto processing = viperbass::guiAudioStatus(state, true, dark);
            const auto bypass = viperbass::guiAudioStatus(state, false, dark);
            assert(processing.bypassNotice.isEmpty());
            assert(bypass.bypassNotice.contains("effects bypassed"));
            assert(processing.greenColor == bypass.greenColor);
            assert(processing.yellowColor == bypass.yellowColor);

            const QColor green(processing.greenColor), yellow(bypass.yellowColor);
            assert(green.isValid() && yellow.isValid());
            assert(green.hue() >= 90 && green.hue() <= 165);
            assert(yellow.hue() >= 35 && yellow.hue() <= 65);
            assert(contrast(green, dark) >= 4.5);
            assert(contrast(yellow, dark) >= 4.5);

            assert(processing.html().contains(processing.greenColor));
            assert(!processing.html().contains(processing.yellowColor));
            assert(bypass.html().contains(bypass.greenColor));
            assert(bypass.html().contains(bypass.yellowColor));
            if (state == BackendAudioActivity::Active) {
                assert(processing.primary.contains("Processing audio"));
                assert(bypass.primary.contains("Audio detected"));
                assert(!bypass.primary.contains("Processing audio"));
            } else {
                assert(processing.primary.contains("Connected"));
                assert(bypass.primary.contains("Connected"));
                assert(!bypass.primary.contains("Processing audio"));
            }
        }
    }
    std::cout << "PASS: green active/connected status, yellow bypass, readable themes\n";
}
