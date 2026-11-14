# RadPro log archive for Home Assistant

A local Home Assistant app that archives the bridge's HTTP runtime log every five
seconds in `/share/radpro-logs`. Open **RadPro logs** in the HA sidebar to inspect
status and download files. No process on a laptop is involved after installation.

Copy this directory into Home Assistant's local apps folder, reload the app store,
install **RadPro Log Archive**, and enable start on boot, watchdog and sidebar.
The options select the bridge address and polling interval. The container needs
only its normal network access and the persistent share directory.

The collector restarts with Home Assistant, resumes its saved log cursor, records
unreachable intervals and buffer overruns, and rotates archives at 4 MiB with a
256 MiB total limit for JSONL files. A fresh installation starts a 48-hour
observation window, writes `soak-summary.json` at its end, then continues logging.
Outages and missing data are included in the result; the deadline alone is not
a successful soak. Panic records are stored separately and retained.

Runtime logs from `/logs.json` are application messages, **not raw UART output**.
Wi-Fi outages can overrun the bridge's 400-entry RAM log; any detected gap is
reported. No polling service can recover messages that were never transmitted.

The accompanying local firmware's panic recorder mirrors IDF's actual panic
printer into a fixed 3,071-byte RTC buffer without allocating memory, writing
flash or reading SYSTIMER in the failing context. After a successful reboot, it
saves the latest valid record to the existing NVS partition. The read-only
`/diagnostics/crash.json` endpoint includes the capture boot ID, original ELF hash,
reset reason, panic text and truncation flag. Home Assistant archives new records
and deduplicates them across subsequent boots. Routine capture causes no flash
writes on the bridge. It does add read-only HTTP requests during the test.

RTC evidence survives a normal software/watchdog reset that retains RTC memory.
It can be lost if power is removed before it is saved, a reset clears RTC memory,
or boot cannot reach the recorder. A hang with no panic output has no backtrace
to retain. The runtime archive will show lost connectivity in that case.
The recorder does not change or feed watchdogs, set the timer counter, or repair
the original intermittent SYSTIMER failure.

Run `python3 test_collector.py` to check cursor/reset handling, buffer-gap reporting
and panic deduplication with local fixtures. Firmware host tests include
`panic_capture_buffer_test.cpp`. A deliberate test abort on the connected bridge
validated capture, reboot persistence, matching ELF hash, and HA archiving; its
record is labeled in the installed archive's `calibration-note.json` and
`calibration-classification.json`. It is not a spontaneous watchdog event.

Preserve the exact ELF alongside crash evidence for each firmware build and match
its SHA256 when decoding a backtrace. An unpublished diagnostic build can share
its version string with a release while having a different ELF hash. The archive
records that hash so findings can be attributed to the correct installed build.

## Safecast buffer regression observation

The local fixed firmware exposes `/diagnostics/safecast.json`. The collector
archives that snapshot with its minute-by-minute telemetry, including actual
build ELF hash, selected unit, history count and capacity drops. Disabled or
paused Safecast retains the latest reading of each unit for test uploads but
collects no averaging history. Enabled collection keeps at most 1,024 selected
unit samples inside the configured time window in a fixed 8 KiB ring. If a
long interval or high sample rate fills the ring, the oldest sample is discarded;
`capacityDrops` reports this and the average covers the retained recent samples.
History resets on unit/interval changes and enable/pause transitions.
No per-sample heap allocation is used, and USB/main-task access is serialized.
