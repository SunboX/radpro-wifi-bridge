# USB reply synchronization

Rad Pro serial replies contain values but no request IDs. After a command times
out, a late numeric reply can otherwise be mistaken for the next command's
result. For example, a cumulative pulse count can be read as counts per minute
and converted into a false dose-rate spike.

On a timeout, the bridge now discards the pending command batch and suspends
normal polling. It sends one `GET deviceId` probe after a short quiet period and
ignores late numeric replies until the matching detector identity is received.
The ordered serial stream makes that distinctive response the boundary between
old replies and the new batch. Detector metadata and calibration are then read
again before normal polling resumes. If identity recovery fails, existing USB
host recovery and the configurable USB recovery watchdog remain available.

Identity responses must contain a nonempty identifier. Startup also sends only
one CRLF-terminated identity query per attempt, rather than sending three
terminator variants and leaving extra replies in the stream.

This is a reply-matching fix, not a filter for large readings. A high count rate
returned for a correctly matched request is still published unchanged.
