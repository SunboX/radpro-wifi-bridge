# USB Recovery

Open **USB Recovery** in the bridge web interface to enable or disable automatic
bridge restarts and choose a timeout from 1 to 60 minutes. The default is **5 minutes**.
Settings are saved immediately and included in configuration backups.

The watchdog starts only after a successful detector telemetry response. If replies stop,
the bridge keeps trying normal USB host recovery and restarts itself when the
response timeout expires. USB reconnects and host restarts do not extend that
deadline; a new detector response does. Detectors plugged in after boot get
their own opening grace period before USB host recovery can interrupt them.

A missing detector at startup does not cause repeated reboots. A detector that
continues replying while switched off also does not trigger this watchdog.
The timeout is at least three configured polling intervals so slow polling does
not cause unnecessary resets. Firmware updates of the bridge or detector suspend
and disarm the watchdog; a new response is required to arm it again afterwards.

The serial log explains the watchdog restart. The next boot also reports
`Previous restart: USB recovery watchdog (detector stopped responding).`

The backup field `usbRecoveryTimeoutSeconds` is 300 by default. Set it to 0 to
disable recovery, or 60–3600 for a timeout in seconds. Older backups without this
field keep the current watchdog setting.
