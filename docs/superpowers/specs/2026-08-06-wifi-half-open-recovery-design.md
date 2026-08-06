# Wi-Fi Half-Open Recovery Design

Date: 2026-08-06
Branch: `main`
Target release: `1.15.13`

## Goal

Fix both Wi-Fi recovery failures reported in GitHub issue #21:

- stop the bridge reconnect loop from interrupting a successful station association before DHCP supplies an IP address
- recover when the ESP32 still reports `WL_CONNECTED` although the local Wi-Fi data path is no longer usable

The release will be built and verified locally, but it will be published without a physical-device test because the maintainer is on holiday and currently has no access to the bridge hardware.

## Investigation Summary

The issue contains two distinct failure sequences.

### Rapid reconnect loop

Arduino-ESP32 2.0.17 enables its own automatic reconnect by default. Release 1.15.12 also added an application-level reconnect loop. The bridge sets `pendingReconnect_` on every `STA_DISCONNECTED` event and resets the retry timestamp to zero. After `STA_CONNECTED`, Arduino does not report `WL_CONNECTED` until `GOT_IP`; during that interval the bridge can call `WiFi.reconnect()` again and tear down the association before DHCP completes. The resulting disconnect event resets the retry delay, causing another immediate attempt.

### Beacon-timeout half-open connection

The follow-up log contains `wifi:bcn_timeout,ap_probe_send_start`, followed by repeated MQTT TCP timeouts, but no `STA_DISCONNECTED` event. Espressif documents the beacon timeout as an intermediate state: the station sends probe requests and only disconnects when those probes also fail. In the reported case Arduino continued returning `WL_CONNECTED`, which is also demonstrated by the MQTT publisher proceeding past its Wi-Fi status guard.

The bridge currently arms recovery only after `STA_DISCONNECTED`. It has no independent reachability check, so a stale association can remain offline indefinitely.

The log cannot determine why the original beacons disappeared. Possible causes include RF conditions, temporary AP behavior, or an ESP-IDF 4.4.7 driver state problem. The firmware fix therefore targets reliable recovery rather than claiming to eliminate the initiating radio event.

## Considered Approaches

### 1. Gateway ICMP reachability monitor with a proven-good baseline

Periodically send one asynchronous ICMP echo to the configured gateway. Do not enforce failures until the gateway has answered successfully at least once for the current IP lease. After three consecutive failures, explicitly tear down the stale station connection and hand control to the bridge reconnect state machine.

Advantages:

- tests the local Wi-Fi path independently of MQTT or any internet service
- does not reconnect merely because a configured publisher is unavailable
- the successful-baseline requirement avoids breaking installations whose routers intentionally ignore ICMP

Trade-off:

- recovery begins after a conservative detection window rather than after the first failed packet

### 2. Treat repeated MQTT failures as Wi-Fi failures

This is simpler, but it conflates broker availability, DNS, authentication, and Wi-Fi health. A broker outage could force needless Wi-Fi reconnects, and installations with MQTT disabled would have no watchdog.

### 3. Reboot after prolonged publisher failure

This provides a broad escape hatch but loses diagnostic state, interrupts USB/device processing, and still cannot distinguish a network outage from an application-service outage.

## Selected Approach

Use approach 1 and make the bridge the sole owner of reconnect timing.

- disable Arduino's automatic reconnect after Wi-Fi setup so the Arduino layer and bridge do not issue competing connection commands
- suppress application reconnect attempts while association is waiting for `GOT_IP`
- preserve the retry timestamp across repeated disconnect callbacks instead of making every callback immediately due
- clear reconnect state explicitly on `GOT_IP`
- monitor the gateway with asynchronous, single-packet ICMP probes
- require one successful gateway probe before enforcing failures for the current IP lease
- recover after three consecutive failed probes

## Detailed Design

### Reconnect state policy

Extend the existing host-testable Wi-Fi reconnect policy with pure decisions for:

- whether a reconnect attempt must wait while DHCP is in progress
- whether a new disconnect should make the first retry immediately due or preserve an already-running retry interval
- when a gateway failure streak becomes actionable

Runtime behavior:

1. `STA_DISCONNECTED` arms reconnect immediately only when reconnect was not already pending.
2. `STA_CONNECTED` records the start of the DHCP wait and prevents another reconnect attempt.
3. `GOT_IP` clears `pendingReconnect_`, the retry timestamp, and the DHCP wait.
4. If `GOT_IP` does not arrive within ten seconds, DHCP waiting expires and the normal five-second reconnect policy resumes.
5. Arduino auto-reconnect is disabled so only this state machine issues reconnect requests.

### Gateway health policy

Add a small host-testable policy object with these states:

- `unproven`: no successful gateway response has been observed for the current lease
- `healthy`: at least one response has been observed and the failure streak is zero
- `suspect`: the gateway was previously proven reachable and one or two consecutive probes failed
- `stale`: the gateway was previously proven reachable and three consecutive probes failed

Rules:

- a success proves the gateway and resets the failure streak
- failures before the first success are diagnostic only and never force recovery
- three consecutive failures after a success request stale-link recovery
- a new IP lease resets the policy to `unproven`
- any Wi-Fi disconnect cancels enforcement until a new `GOT_IP`

### Gateway probe runtime

Use ESP-IDF's asynchronous `esp_ping` session API:

- target: `WiFi.gatewayIP()`
- packet count: one
- timeout: 1000 ms
- interval between probes: 30 seconds
- no blocking work in the Arduino loop

The ping callbacks only record completion and success. The main loop consumes the result, updates the pure policy, and performs all logging and Wi-Fi state changes.

If the ping session cannot be created or started, log the ESP error and retry at the next interval. Session-setup errors do not count as gateway failures.

When the policy reports a stale link:

1. log the failure count and gateway address
2. stop treating the cached `WL_CONNECTED` value as healthy
3. arm bridge reconnect state
4. explicitly disconnect the station without erasing saved credentials
5. let the normal reconnect policy rejoin the saved network

### Logging and diagnostics

Add concise state-transition logs rather than one line for every successful periodic probe:

- gateway monitor armed after its first successful probe
- gateway probe failure count while armed
- stale link detected and forced recovery started
- DHCP wait timed out
- ping session creation/start failure with the ESP error name

Successful probes after the baseline remain silent unless they recover a prior failure streak.

## Error Handling and Safety

- Router does not answer ICMP: the monitor never gets a successful baseline and therefore never forces recovery.
- Gateway address is zero or unavailable: skip probing until a valid `GOT_IP` event supplies one.
- Ping allocation/socket failure: log and retry later; do not classify the link as stale.
- Wi-Fi disconnect during a probe: ignore the completed result for health enforcement and reset on the next IP lease.
- DHCP is slow: wait up to ten seconds before allowing another reconnect command.
- Gateway recovers after one or two missed probes: reset the failure streak without disconnecting Wi-Fi.
- Persistent gateway failure after a proven-good baseline: force one disconnect, reset health state, and rely on the existing bounded reconnect interval.
- Saved credentials remain intact throughout recovery.

## Testing

Follow a red-green test cycle before implementation.

Host regression tests will cover:

- reconnect is suppressed between `STA_CONNECTED` and `GOT_IP`
- DHCP waiting expires after ten seconds
- repeated disconnect callbacks preserve an active retry delay
- `GOT_IP` clears pending reconnect state
- gateway failures before the first success never trigger recovery
- a success arms gateway enforcement
- one and two post-baseline failures remain non-actionable
- the third consecutive post-baseline failure triggers stale-link recovery
- a success resets a failure streak
- a new lease resets the successful baseline

Build verification will include:

- all repository host tests
- PlatformIO `buildprog`
- PlatformIO `buildfs`
- inspection of generated version metadata and the `firmware_1.15.13.zip` contents
- comparison of the committed source state with the generated firmware version string

Physical-device behavior will remain explicitly unverified for this release because no device is available during the maintainer's holiday.

## Release and Issue Communication

Release 1.15.13 will include:

- a detailed explanation of the two failure modes
- the gateway-monitor design and false-positive protection
- the reconnect/DHCP race correction
- local test and build evidence
- a clear statement that the release has not yet been tested on physical hardware

The GitHub issue reply will be written as a short, direct maintainer update in André's established style: thank the reporter, explain the investigation and fix in concrete terms, link the release, state that hardware testing was not possible while on holiday, and ask the reporter to test and share a log if possible.

## Non-Goals

- claiming the firmware can identify the original RF or access-point cause from the supplied log
- treating cloud-service or MQTT outages as Wi-Fi failures
- restarting the whole ESP32 as the first recovery action
- erasing or replacing stored Wi-Fi credentials
- upgrading the Arduino-ESP32 or ESP-IDF toolchain in the same release
