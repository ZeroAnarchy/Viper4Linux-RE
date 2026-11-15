#pragma once
// Connected GUI status presenter. A source of truth for semantic status and
// visible color roles, shared by live GUI code and its isolated regression test.
#include "viper-backend-state.h"
#include <QString>

namespace viperbass {
struct GuiAudioStatus {
    QString primary;
    QString bypassNotice;
    QString greenColor;
    QString yellowColor;

    QString html() const {
        QString rendered = QStringLiteral("<span style=\"color:%1;\">%2</span>")
            .arg(greenColor, primary.toHtmlEscaped());
        if (!bypassNotice.isEmpty())
            rendered += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                .arg(yellowColor, bypassNotice.toHtmlEscaped());
        return rendered;
    }
};

inline GuiAudioStatus guiAudioStatus(BackendAudioActivity state,
                                    bool masterEffectsEnabled, bool darkTheme) {
    GuiAudioStatus status;
    // Both have high contrast against their intended background. Use a
    // green main message in every connected state, and golden-yellow only
    // for bypassed effects. Do not use a global QLabel color override.
    status.greenColor = darkTheme ? "#63D995" : "#167345";
    status.yellowColor = darkTheme ? "#FFD45C" : "#8A7200";
    switch (state) {
    case BackendAudioActivity::Active:
        status.primary = masterEffectsEnabled
            ? QStringLiteral("● Processing audio — input signal detected")
            : QStringLiteral("● Audio detected");
        break;
    case BackendAudioActivity::NoSignal:
        status.primary = masterEffectsEnabled
            ? QStringLiteral("○ Connected — no audio signal (DSP ready)")
            : QStringLiteral("○ Connected — no audio signal");
        break;
    case BackendAudioActivity::Idle:
        status.primary = masterEffectsEnabled
            ? QStringLiteral("○ Connected — idle (effects processor ready)")
            : QStringLiteral("○ Connected — idle");
        break;
    case BackendAudioActivity::Checking:
        status.primary = QStringLiteral("○ Connected — checking audio activity…");
        break;
    }
    if (!masterEffectsEnabled)
        status.bypassNotice = QStringLiteral(" — effects bypassed");
    return status;
}
} // namespace viperbass
