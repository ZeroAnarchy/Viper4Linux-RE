#!/usr/bin/env python3
"""Keep only safety-critical tray and Preferences lifecycle guards."""
from pathlib import Path
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
assert "app.setQuitOnLastWindowClosed(false);" in source
assert "window.showAtStartup();" in source
assert "QSystemTrayIcon::isSystemTrayAvailable()" in source
# Cold-start tray readiness: immediate success is fast, late readiness stays
# hidden, and a genuinely tray-less desktop gets a bounded visible fallback.
startup = source[source.index("    void showAtStartup() {"):
                 source.index("protected:", source.index("    void showAtStartup() {"))]
assert 'if (choice == "tray") {' in startup
assert 'if (trayAvailable()) return;' in startup
assert "startup_tray_fallback_pending_ = true;" in startup
assert "QTimer::singleShot(2000, this, [this] {" in startup
assert "if (!startup_tray_fallback_pending_) return;" in startup
assert "startup_tray_fallback_pending_ = false;" in startup
assert "if (trayAvailable() || isVisible()) return;" in startup
assert startup.index('if (trayAvailable()) return;') < startup.index('QTimer::singleShot(2000')
assert startup.index('QTimer::singleShot(2000') < startup.index('if (trayAvailable() || isVisible()) return;')
assert startup.index('if (trayAvailable() || isVisible()) return;') < startup.index('show();')
assert "startup_tray_fallback_pending_ = false;" in source[source.index("    void restoreFromTray() {"):
                                                              source.index("    // Called only on actual GUI exit")]
assert "startup_tray_fallback_pending_ = false;" in source[source.index("    void closeEvent(QCloseEvent* event) override {"):
                                                              source.index("    void changeEvent(QEvent* event) override {")]

# Preferences cancellation must not apply changes, but may persist geometry.
prefs = source[source.index("    void showPreferences() {"):source.index("    QString logRetentionHelper() const {")]
assert prefs.index('settings_.setValue("ui/preferencesDialogSize", dialog.size());') < prefs.index("if (outcome != QDialog::Accepted) return;")
assert prefs.index("if (outcome != QDialog::Accepted) return;") < prefs.index('settings_.setValue("ui/startupBehavior"')

# Never hide the window if the tray cannot accept it.
assert "System tray unavailable; showing the main window instead." in source
handler = source[source.index("    void changeEvent(QEvent* event) override {"):
                 source.index("    bool eventFilter(QObject* watched, QEvent* event) override {")]
assert "tray_restore_maximized_ = was_maximized;" in handler
assert "hide();" in handler
assert "setWindowState(" not in handler
assert "force_taskbar_minimize_" in handler
# A forced minimize (startup or close-to-taskbar) stays exempt until restored.
# A zero-delay reset used to race the WM on Cinnamon/X11 and hide it in the tray.
startup = source[source.index("    void showAtStartup() {"):
                 source.index("protected:", source.index("    void showAtStartup() {"))]
close = source[source.index("    void closeEvent(QCloseEvent* event) override {"):
               source.index("    void changeEvent(QEvent* event) override {")]
assert "force_taskbar_minimize_ = true;" in startup
assert "force_taskbar_minimize_ = true;" in close
assert "force_taskbar_minimize_ = false;" not in startup + close
# A remembered maximized window must not schedule a delayed show that undoes
# the explicitly selected start-minimized mode (observed on native Wayland).
layout = (root / "src/window/window-layout.inc").read_text()
sizes = layout[layout.index("    void restoreWindowSizes() {"):
               layout.index("    static bool runningCinnamon() {")]
assert 'settings_.value("ui/mainWindowMaximized", false).toBool()' in sizes
assert "setWindowState(windowState() | Qt::WindowMaximized);" in sizes
assert "showMaximized();" not in sizes
assert "QTimer::singleShot(" not in sizes
assert 'showMinimized();' in startup
# On Wayland the compositor may ignore minimization requested before the
# first xdg_toplevel mapping. Re-request once if the new window is exposed;
# never retry from a timer alone or change X11 restoration.
assert 'QGuiApplication::platformName() == QLatin1String("wayland")' in startup
assert "nativeWindow->installEventFilter(this);" in startup
assert "if (nativeWindow->isExposed())" in startup
filter_section = source[source.index("    bool eventFilter(QObject* watched, QEvent* event) override {"):
                        source.index("private:", source.index("    bool eventFilter(QObject* watched, QEvent* event) override {"))]
assert "event->type() == QEvent::Expose" in filter_section
# gui-startup-notification.h brings in Xlib's numeric Expose macro; without
# the explicit undef the Wayland handler fails to compile on Linux.
main = (root / "src/viper-gui.cpp").read_text()
assert '#include "gui-startup-notification.h"' in main
assert '#ifdef Expose\n#undef Expose\n#endif' in main
assert main.index('#include "gui-startup-notification.h"') < main.index('#undef Expose')
assert main.index('#undef Expose') < main.index('event->type() == QEvent::Expose')
assert "scheduleWaylandStartupMinimizeRetry();" in filter_section
wayland_retry = source[source.index("    void scheduleWaylandStartupMinimizeRetry() {"):
                       source.index("    void scheduleX11TaskbarRestoreSettle() {")]
assert "wayland_startup_minimize_retry_scheduled_" in wayland_retry
assert "QTimer::singleShot(0, this" in wayland_retry
assert "if (!wayland_startup_minimize_pending_ || !isVisible()) return;" in wayland_retry
assert "nativeWindow->isExposed()" in wayland_retry
assert "nativeWindow->windowStates() | Qt::WindowMinimized" in wayland_retry
assert "wayland_startup_minimize_pending_ = false;" in wayland_retry
restore_section = source[source.index("    void restoreFromTray() {"):
                         source.index("    // Called only on actual GUI exit")]
close_section = source[source.index("    void closeEvent(QCloseEvent* event) override {"):
                       source.index("    void changeEvent(QEvent* event) override {")]
assert "wayland_startup_minimize_pending_ = false;" in restore_section
assert "wayland_startup_minimize_pending_ = false;" in close_section
# Wayland does not retain a reliable minimized state. A tray toggle must
# honor our own minimized-to-taskbar requests even if isMinimized() is false.
assert "rememberWaylandTaskbarMinimize();" in startup
assert "rememberWaylandTaskbarMinimize();" in close
remember = source[source.index("    void rememberWaylandTaskbarMinimize() {"):
                  source.index("    void scheduleWaylandStartupMinimizeRetry() {")]
assert 'QGuiApplication::platformName() == QLatin1String("wayland")' in remember
assert "wayland_taskbar_minimize_requested_ = true;" in remember
assert "wayland_taskbar_was_maximized_ = isMaximized();" in remember
assert "windowHandle() && !windowHandle()->isExposed()" in remember
assert "waylandTaskbarNeedsRestore()" in remember
assert "wayland_taskbar_saw_inactive_ = false;" in remember
assert "wayland_taskbar_minimize_requested_ = false;" in restore_section
assert "if (waylandTaskbarRestore) {" in restore_section
assert "if (waylandRestoreMaximized) showMaximized();" in restore_section
assert "else showNormal();" in restore_section
assert "wayland_taskbar_minimize_requested_ = false;" in close
assert "wayland_taskbar_saw_inactive_" in handler
assert "wayland_taskbar_saw_inactive_" in filter_section
toggle = source[source.index("    void toggleWindowFromTray() {"):
                source.index("    // Called only on actual GUI exit")]
assert "if (!isVisible() || isMinimized() || waylandTaskbarNeedsRestore())" in toggle
assert "restoreFromTray();" in toggle
assert "saveUiGeometry();" in toggle and "hide();" in toggle
tray_handlers = source[source.index("    void setupTray() {"):
                       source.index("    QString autostartFile() const {")]
assert tray_handlers.count("toggleWindowFromTray();") == 2
assert "if (isVisible() && !isMinimized())" not in tray_handlers
assert "window.showAtStartup();" in source
assert "event->type() == QEvent::ActivationChange" in handler
assert "isVisible() && !isMinimized() && isActiveWindow()" in handler
assert "if (is_state_change && !isMinimized())" not in handler
assert "x11_taskbar_restore_pending_" in handler
assert "if (is_state_change && isMinimized()) {" in handler
assert "retryX11TaskbarRestore(generation);" in handler
assert "scheduleX11TaskbarRestoreSettle();" in handler
assert "force_taskbar_minimize_ = false;" in handler
# Deferred hides must not outlive an explicit user request to restore.
assert "const quint64 generation = tray_restore_generation_;" in handler
assert "generation != tray_restore_generation_ || force_taskbar_minimize_" in handler
assert "++tray_restore_generation_;" in source
# Explicit reactivation keeps delayed WM minimization events from re-hiding
# a window that was closed to the tray directly from its minimized taskbar item.
restore = source[source.index("    void restoreFromTray() {"):
                 source.index("    // Called only on actual GUI exit")]
assert "if (isMinimized()) {" in restore
assert restore.index("force_taskbar_minimize_ = true;") < restore.index("else {\n                showNormal();")
# Only visible X11 taskbar restoration combines the WM's restore and activate
# operations; the pre-existing hidden-to-tray path stays separate.
assert 'const bool taskbarRestore = isMinimized() && isVisible() &&' in restore
assert 'QGuiApplication::platformName() == QLatin1String("xcb")' in restore
assert 'setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);' in restore
assert 'if (!taskbarRestore) activateWindow();' in restore
assert "x11_taskbar_restore_pending_ = true;" in restore
assert "scheduleX11TaskbarRestoreSettle();" in restore
assert "cancelX11TaskbarRestore();" in restore
settle = source[source.index("    void scheduleX11TaskbarRestoreSettle() {"):
                source.index("    void restoreFromTray() {")]
assert "QTimer::singleShot(450, this" in settle
assert "generation != tray_restore_generation_" in settle
assert "epoch != x11_taskbar_restore_settle_epoch_" in settle
assert "force_taskbar_minimize_ = false;" in settle
assert "x11_taskbar_restore_retries_ > 3" in settle
assert "setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);" in settle
# Both tray entry points use the shared toggle; when toggling a visible
# window into the tray, cancel any pending X11 taskbar restoration first.
assert tray_handlers.count("toggleWindowFromTray();") == 2
assert "cancelX11TaskbarRestore();" in toggle
assert toggle.index("cancelX11TaskbarRestore();") < toggle.index("hide();")
assert "cancelX11TaskbarRestore();" in close

# Diagnostic window-state tracing is not part of the released GUI.
assert "traceTray(" not in source
assert "VIPER_GUI_TRAY_TRACE" not in source

# Native Wayland must remain native unless the user selects XWayland.
assert 'qputenv("QT_QPA_PLATFORM", "xcb");' not in main
assert 'const bool x11Gui = QGuiApplication::platformName() == QLatin1String("xcb");' in prefs
assert "minimize->setEnabled(x11Gui);" in prefs
assert 'QGuiApplication::platformName() != QLatin1String("xcb")' in handler

# Tray and close actions must never directly manage the audio backend.
for begin, end in (
    ("    void setupTray() {", "    QString autostartFile() const {"),
    ("    void closeEvent(QCloseEvent* event) override {",
     "    bool eventFilter(QObject* watched, QEvent* event) override {"),
):
    section = source[source.index(begin):source.index(end)]
    for forbidden in ("ensureBackend(", "QProcess::start(", "start.sh",
                      "stop.sh", "publishToBackend(", "running_clients("):
        assert forbidden not in section, (begin, forbidden)
print("PASS: tray fallback, Preferences cancel, Wayland and backend isolation")
