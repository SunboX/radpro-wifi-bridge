# Wi-Fi recovery and network waits

The bridge uses one reconnect state machine. It gives association and DHCP up to
10 seconds per attempt, preserves its retry delay across repeated disconnect
callbacks, and retries saved credentials while the recovery portal is active.
The local gateway is probed every 30 seconds after it has answered at least once.
Three failed probes force a reconnect even if the Wi-Fi driver still reports a
connection. A router that never answers ICMP does not arm this monitor.

Outbound publishers must not prevent recovery from running. The cooperative
response-wait callback services the Wi-Fi recovery state machine, the web portal,
and the existing MQTT connection. It does not recursively publish another batch.
OpenSenseMap TCP and socket waits are limited to 3 seconds and its TLS handshake
to 5 seconds. Publishers close completed responses instead of waiting indefinitely
for the remote server to close a connection whose remaining body is unused.

USB response callbacks queue MQTT values for the main loop. Repeated values of
the same command type replace the pending value, keeping the backlog bounded
while a network request is in progress. Only the main loop writes MQTT packets,
so telemetry cannot interrupt a streamed discovery packet.

These changes address network waits and reconnect servicing. They do not establish
a fix for the separate SYSTIMER/interrupt-watchdog stalls reported in issue #21.
A short successful reconnect test cannot rule out a failure that previously took
days to appear. Diagnose such a failure with a continuous serial log including
the reset reason and panic backtrace.
