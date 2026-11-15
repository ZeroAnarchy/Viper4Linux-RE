# User guide

## Running ViPER for Linux

Launch the installed application with `viper4linux`. It checks the installed
backend fingerprint (DSP module, controller and PipeWire routing configuration),
reuses a matching running backend, and replaces an older marked ViPER backend
when necessary. This does **not** restart PipeWire itself. GUI-only updates
should leave the backend and music playing uninterrupted.

The main window has six effect tabs. Each effect has an independent switch:
collapsing a panel does not disable its processing. The **Master Limiter** has no independent OFF switch: it remains active whenever **Master** is ON. Turning **Master OFF** bypasses the entire ViPER DSP stage (including limiter, output gain and balance) after a short transition, returning the source audio unchanged apart from nonfinite-sample sanitization. **Theme** selects
Modern Teal or Classic Viper and follows supported desktop light/dark settings.
The compact **☰ menu** provides Preferences, IRS/VDC library shortcuts,
backend log, Effect Guide, About, Presets, and two explicit exit choices:
**Quit GUI & Leave ViperDSP Running** and
**Quit GUI & Close ViperDSP Completely**.
The main-window footer has a separate **Reset** button next to **Presets…**.
Reset restores every effect and slider to its default value, enables Master
processing, and disables all optional effects. A prominent warning recommends
saving the current configuration as a preset; **Cancel** is the default action.
Existing saved presets, GUI preferences, and correction-library files are preserved.

## Window, tray, and login options

Open **☰ → Preferences** to configure:

- Startup: show window, start minimized to the taskbar, or start hidden in the tray.
- Title-bar minimize: normal taskbar minimize or minimize to tray.
- Close (X): quit the GUI, minimize to taskbar, or hide to tray.
- When quitting through close (X): ask every time (default), always keep the
  backend running, or stop the backend. Explicit application-menu and tray-menu
  exit actions override this setting for that exit without changing it.
- Optional login autostart using the user's XDG autostart directory.

Preferences are grouped into **Startup**, **Window & tray behavior**, **GUI exit & audio backend**, and **Audio Output memory**. The dialog can be resized and remembers its size on OK or Cancel. On shorter screens, settings scroll while the action buttons remain visible. **Restore Defaults** resets the dialog's selections to their defaults; changes are applied only after OK, and existing Audio Output snapshots and named presets are not deleted.

Both the application menu and tray menu provide the two explicit exit actions:
**Quit GUI & Leave ViperDSP Running** and
**Quit GUI & Close ViperDSP Completely**. Neither prompts for a backend
choice or changes the remembered quit preference. The tray menu also provides
Show/Hide, the Master processing toggle, and Preferences. The tray click action toggles window visibility. If Qt reports
no usable system tray, the application should keep its window accessible rather
than hide it permanently. On Cinnamon/X11 the black taskbar-window regression
was fixed by not clearing the minimized state after hiding the window.
KDE Plasma tray behavior remains to be verified on a real KDE session.

When quitting through close (X), if the configured exit policy is
**Ask every time** and the ViPER backend is running, the GUI
prompts to **Keep backend running**, **Stop backend**, or **Cancel**. Check
**Remember this choice** to skip future prompts; to change it later, return
to **Preferences → When quitting the GUI** and select **Ask every time**,
**Keep audio backend running**, or **Stop audio backend**.
Choosing Stop signals only the explicitly marked ViPER filter-chain client,
**never the system PipeWire daemon**. If the backend cannot be safely identified
or signaled, the GUI stays open and shows an error instead of guessing.
When no backend is running, the GUI quits without prompting. Closing to
taskbar or tray does not prompt or affect audio. On forced termination
(SIGKILL, crashes, terminal job-control signals), no interactive quit dialog
can be shown. To verify login autostart you only need to log out and back in,
not reboot.

## CLI and troubleshooting

The installed `viper4linux` launcher is now a symbolic link to the native
controller, not a separate Python application launcher. The native CLI provides
three standalone lifecycle commands:

```sh
v4l-cli --quit-gui        # Close only the GUI; leave ViPER DSP processing audio
v4l-cli --quit-all        # Close GUI and stop the marked ViPER backend
v4l-cli --restart-backend # Restart ViPER backend and Audio Output monitor
```

The restart command does not restart the system PipeWire daemon or create
a second GUI. A successful exit waits for the new Audio Output monitor's
fresh heartbeat. CLI-driven quit operations do not alter remembered
GUI exit preferences.

The installed native controller is `viper4linux-ctl`, with the shorter `v4l-cli`
symlink providing the same commands and options. Try:

```bash
v4l-cli --status
v4l-cli --status --full
v4l-cli --off                     # Bypass Master processing
v4l-cli --on                      # Enable Master processing
v4l-cli --bass on gain=3 freq=65  # Enable and adjust ViPER Bass
v4l-cli --headphone on level=2    # Headphone Surround+ Level 2
v4l-cli --preset list
v4l-cli --preset --help
v4l-cli --help
man viper4linux-ctl
```
The longer `viper4linux-ctl` command accepts the same arguments.
Use `v4l-cli --EFFECT --help` (for example, `--deq --help`) for
effect-specific choices and ranges.

Backend status distinguishes the following conditions:

- **Processing audio / Audio detected (GUI):** DSP callbacks are active and
  the incoming audio signal exceeded about -80 dBFS in recent blocks.
  With Master ON, the status reads **Processing audio — input signal detected**
  in green. With Master OFF, it reads **Audio detected** in green followed by
  **effects bypassed** in yellow. This detects input samples, **not**
  necessarily audible output (muting or output routing can still prevent sound).
  A short ~1.5-second grace reduces flicker during rests.
- **Connected — no audio signal (GUI):** DSP callbacks continue under
  `node.always-process = true`, but no input above the detection floor was
  seen recently. Connected/readiness text is green; with Master OFF the
  yellow **effects bypassed** suffix also appears. Paused or silent players
  should reach this state.
- **Connected — idle (GUI):** the DSP channel is present but its audio callback
  counter has not advanced since the previous GUI poll; effects are ready.
- **Connected — checking audio activity (GUI):** the first sample after attaching
  or restarting the DSP. A second sample is needed to classify signal versus
  no signal.

- **Ready to process audio:** the dedicated, marked PipeWire backend process is
  running, but its DSP control channel has not appeared yet. The smart filter
  is configured to initialize the DSP even without music, so this should be
  transient at startup rather than requiring playback. The status command
  returns success (exit code 0), but effect-setting commands fail (exit code 1)
  until the control channel exists. A persistent state warrants checking the
  backend log and filter-chain configuration.
- **Offline:** no marked ViPER backend process is detected. Start `viper4linux`.

The application menu groups **View ViPER Log** with **Log Retention…**.
Both backend and Audio Output monitor diagnostics share one timestamped file,
`backend.log`, with source/PID labels distinguishing the processes.
Retention defaults to **7 days** for this combined log.
Choose any period from 1 to 3650 days, or **0** to retain indefinitely.
Changing the setting cleans older entries immediately; automatic maintenance
also runs about once per hour while the Audio Output monitor is operating.
Log cleanup leaves the audio backend running. The separate
`output-monitor.log` from older releases is imported once, after the old
monitor stops; its historical entries are marked and preserved.
Untimestamped historical entries
preceding the new logging format cannot be aged precisely: they are
conservatively treated as belonging to the first subsequent timestamped
entry. The duration is a retention period, not a fixed disk-space quota.

`v4l-cli --status` is instantaneous and reports the current callback
counter rather than claiming active/idle from a single sample. Run it twice
if you need to check whether the number is increasing. The GUI observes changes
automatically. Closing a music player does not unload or restart the DSP.
Other applications can still supply input to ViPER when Elisa is paused.

If audio is playing but the status remains **Ready to process audio**, inspect
routing and the backend log: a process can be alive without a working DSP.

The CLI writes **live shared-memory controls** and immediately saves
successful changes to the same per-user Qt settings store as the GUI, even when
the GUI is closed. It manages complete named effect presets with
`--preset` (list) and `--preset --help` (also `--preset help`: save, overwrite, load, rename and
delete syntax), imports/selects IRS/VDC files, and exposes a standalone
`--reset-effects` command. Unlike the GUI button, the CLI reset is
noninteractive: save your current configuration first if needed. If the
settings store is unwritable, the command reports a persistence failure even
though the live DSP change may already have taken effect. GUI and CLI changes
may supersede each other.

Use `--on` and `--off` for the global Master bypass; use `--bass on`
and `--bass off` for ViPER Bass alone. New effect-scoped invocations use
`--effect option=value`; old value-taking positional forms are not supported
in this grammar. Some historical no-argument aliases remain for scripts, but
new commands and automation should use the scoped syntax. All arguments in
a sequence are prevalidated before editing DSP state; runtime failures after
an earlier successful edit are not automatically rolled back.

For Headphone Surround+, the CLI uses the **same Level 1 through Level 5**
numbering shown by the GUI. For instance, `v4l-cli --headphone level=2` selects GUI Level 2
(the backend stores index 1). The old `headphone-quality` name was retired.
Invalid values return exit status 2 without changing the current level.
Use `v4l-cli --status headphone` to inspect the live level before and
after setting it.

## Audio Output memory and preset assignment

**Preferences → Remember settings per Audio Output** enables independent DSP
settings for each positively identified physical output. The monitored
selection follows PipeWire's physical sink and continues after the GUI closes.
The monitor waits for the output to settle before saving or restoring settings;
switching outputs while playback is paused is supported.

Open **☰ → Presets…** to save complete effect settings. **Save Preset…**
defaults to **New Preset…**, or you can pick an existing preset for a confirmed
overwrite. To assign a saved preset to internal speakers, headphones, or a
remembered disconnected output, select it and choose **Apply to Audio Output…**.
Confirm the target device. If it's active, settings are applied immediately;
otherwise, the existing audio stays unchanged until that output becomes active.
Saved output settings subsequently evolve independently of the named preset.

For an inspection of current outputs use `viper4linux-output --json`.
The companion `viper4linux-output --enable-memory` and `--disable-memory`
switches control the opt-in store; `--assign-preset FILE --target OUTPUT_KEY`
uses the monitored, acknowledged assignment path. The keys are exact,
remembered physical identifiers, not display-name matches.

For a direct-source development build, useful scripts are `./scripts/status.sh`,
`./scripts/verify.sh`, `./scripts/stop.sh` and `./scripts/start.sh`. The source installer builds
`build/viper-ctl`, `build/viper-gui`, and `build/viper_dsp_ladspa.so`.

Backend startup logs are under `$XDG_STATE_HOME/viper4linux/backend.log`
(normally `~/.local/state/viper4linux/backend.log`). Verify that the expected
PipeWire sink is active and that the audio-block counter in CLI status advances
while audio plays. If routing becomes ineffective while JamesDSP is active,
check routing before assuming an effect implementation has failed.

## Effect and file guidance

See [Effects](EFFECTS.md) for processor behavior and restrictions. For
importing `.irs` and `.vdc` files, see [Presets and libraries](PRESETS-AND-LIBRARIES.md).

## Automating effects with the CLI

`v4l-cli` can control all 24 effect groups without starting the GUI.
The unscoped Master bypass commands are distinct from per-effect `on`/`off`.
Options inside an effect scope use `name=value`; multiple scopes can be
combined into a single invocation.

```sh
v4l-cli --on --bass on gain=3 freq=65 --headphone on level=2
v4l-cli --eq bands=31 level=9:-2
v4l-cli --deq --help
v4l-cli --imager --help
v4l-cli --preset save "Before experiments"
v4l-cli --preset load "Before experiments"
```
To select or import correction files, use `--ddc file=PATH`,
`--ddc import=PATH`, `--convolver file=PATH`, or
`--convolver import=PATH`. These standalone preset operations are not
mixed into effect-setting batches.

The interface applies changes to the **running** backend. An open GUI reflects
them on its 500 ms polling cycle and saves changed controls. When the GUI is closed, successful CLI changes still save directly to the
GUI's Qt preferences; they survive a backend restart without reopening the
GUI. The CLI waits for affected native workers to acknowledge changes, with
a bounded timeout. A timeout must not be mistaken for confirmed application
of an effect.

The CLI returns status `0` for a successful command, `1` if no backend is
running, and `2` for invalid commands or arguments. See
`man viper4linux-ctl` for the available switches and argument ranges.
