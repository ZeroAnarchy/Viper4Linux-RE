#!/usr/bin/env python3
"""Keep only safety-critical tray and Preferences lifecycle guards."""
from pathlib import Path
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
assert "app.setQuitOnLastWindowClosed(false);" in source
assert "window.showAtStartup();" in source
assert "QSystemTrayIcon::isSystemTrayAvailable()" in source
assert "if (!trayAvailable()) return;" in source

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
assert "cancelX11TaskbarRestore();" in source[source.index("    void setupTray() {"):
                                             source.index("    QString autostartFile() const {")]
assert "cancelX11TaskbarRestore();" in close

# Diagnostic window-state tracing is not part of the released GUI.
assert "traceTray(" not in source
assert "VIPER_GUI_TRAY_TRACE" not in source

# Native Wayland must remain native unless the user selects XWayland.
main = (root / "src/viper-gui.cpp").read_text()
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
