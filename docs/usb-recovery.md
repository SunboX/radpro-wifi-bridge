# USB Recovery

Open **USB Recovery** in the bridge web interface to enable or disable automatic
bridge restarts and choose a timeout from 1 to 60 minutes. The default is **5 minutes**.
Settings are saved immediately and included in configuration backups.

If the USB driver reports that its bulk input transfer stopped on a working
detector, the bridge logs the error and restarts the USB host to resume reads.

The watchdog starts only after a successful detector telemetry response. If replies stop,
the bridge keeps trying normal USB host recovery and restarts itself when the
response timeout expires. If USB has just detected a device at that point,
the bridge allows up to 15 more seconds to open it and receive telemetry.
If the application missed a whole polling window while USB stayed connected,
it immediately retries the detector and allows the same 15 seconds for a reply.
Repeated attachments or scheduling delays share that single window. A successful telemetry
reply starts a fresh timeout. Detectors plugged in after boot also get time
to open before USB host recovery can interrupt them.

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
