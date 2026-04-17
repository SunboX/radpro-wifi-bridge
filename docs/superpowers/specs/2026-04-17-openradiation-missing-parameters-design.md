# OpenRadiation Missing Parameters Design

Date: 2026-04-17
Branch: `OpenRadiation`

## Goal

Extend the existing OpenRadiation integration so the bridge can publish more of the supported measurement metadata without inventing values the device does not actually know.

The target outcome is:

- persist two new OpenRadiation settings in the bridge portal:
  - `measurementEnvironment`
  - `measurementHeight`
- publish these additional OpenRadiation fields when valid:
  - `measurementEnvironment`
  - `measurementHeight`
  - `altitudeAccuracy`
  - `endTime`
  - `hitsNumber`
- seed the live bridge at `192.168.1.59` with a sensible default configuration after implementation and verify the fields appear in OpenRadiation responses.

## Current Context

- The bridge already publishes the core OpenRadiation fields from `OpenRadiationPublisher::buildPayload()`: apparatus ID, value, `startTime`, latitude, longitude, altitude, accuracy, apparatus firmware, sensor type, report UUID, manual-reporting flag, organisation-reporting string, and report context.
- OpenRadiation settings are rendered inline by `WiFiPortalService::sendOpenRadiationForm()` and persisted through `AppConfig` and `AppConfigStore`.
- The bridge already polls and stores cumulative pulse count data (`tubePulseCount`) plus live dose/rate data through `DeviceManager` and `DeviceInfoStore`.
- The OpenRadiation submit API accepted a live `reportContext: "test"` payload containing `altitudeAccuracy`, `measurementHeight`, `measurementEnvironment`, `hitsNumber`, and `endTime` on 2026-04-17.

## Constraints

- Do not add fields that this bridge cannot populate honestly.
- Do not repurpose smartphone-specific metadata for the bridge.
- Keep the implementation inside the existing OpenRadiation config and publisher path.
- Preserve compatibility with existing saved configurations and backup import/export.
- If a derived value cannot be computed safely, omit it from the payload instead of sending guessed data.

## Selected Approach

Use the smallest honest extension:

- add `measurementEnvironment` as a controlled dropdown in the OpenRadiation portal
- add `measurementHeight` as a numeric input in the OpenRadiation portal
- derive `altitudeAccuracy` from the existing position-accuracy field
- compute a bounded measurement interval using queued publish state:
  - `startTime` at queue time
  - `endTime` at submit time
  - `hitsNumber` as the delta of cumulative pulse count across that interval

This approach improves the OpenRadiation record quality without adding speculative metadata or turning the portal into a generic free-form metadata editor.

## Design

### 1. Config Model

Add two persisted OpenRadiation settings to `AppConfig`:

- `openRadiationMeasurementEnvironment`
- `openRadiationMeasurementHeight`

Persistence requirements:

- store both in NVS through `AppConfigStore`
- export both in `/backup.json`
- import both from backup JSON
- keep defaults empty / zero so existing installs remain valid

Validation rules:

- `measurementEnvironment` must be one of the OpenRadiation-supported enum values:
  - `countryside`
  - `city`
  - `ontheroad`
  - `inside`
  - `plane`
- `measurementHeight` is optional and must be clamped to a sane non-negative range
  - fixed range: `0.0` to `100.0` meters

### 2. Portal UX

Extend the existing `/openradiation` page with:

- a dropdown labeled for measurement environment
- a numeric input labeled for measurement height above ground in meters

Portal behavior:

- retain the current page structure and inline-rendered form
- show the new controls near the existing location fields
- save them through the same POST handler as the rest of the OpenRadiation settings
- do not introduce a second page or a separate advanced-settings mode

User-facing values for the dropdown:

- `Countryside` -> `countryside`
- `City` -> `city`
- `On the road` -> `ontheroad`
- `Inside` -> `inside`
- `Plane` -> `plane`

### 3. Publisher Data Model

Keep the current OpenRadiation publisher flow, but extend it with a small queued-measurement window.

When a publishable measurement is queued:

- if system time is valid, capture a queue-time ISO `startTime`
- if system time is not valid yet, keep the publish queued and wait to arm the measurement window until a later loop iteration when time is valid
- capture the current cumulative pulse count, if present and parseable, at the same moment the measurement window is armed

When the measurement is actually submitted:

- keep the queued timestamp as `startTime`
- generate a fresh submit-time ISO timestamp as `endTime`
- read the current cumulative pulse count
- compute `hitsNumber = endPulseCount - startPulseCount` when:
  - both counts are parseable
  - `endPulseCount >= startPulseCount`

If any of those conditions fail:

- omit `hitsNumber`
- still submit the rest of the measurement

This keeps the reported counts tied to a real interval instead of a point measurement pretending to have interval data.

### 4. Payload Composition

The payload continues to send the current stable fields and conditionally adds:

- `measurementEnvironment` when configured
- `measurementHeight` when greater than zero
- `altitudeAccuracy` when position accuracy is greater than zero
- `endTime` when a valid submit timestamp is available
- `hitsNumber` when a valid pulse delta exists

Intentional omissions:

- no `distanceTravelled`
- no `measurementSpeed`
- no weather fields
- no `temperature`
- no tags or description
- no smartphone metadata
- no guessed tube-type field

### 5. Live Bridge Defaults

After implementation and OTA deployment to `192.168.1.59`, seed the device configuration with:

- `measurementEnvironment = city`
- `measurementHeight = 1.0`

Reasoning:

- the configured station coordinates resolve to a residential address in Plauen, so `city` is a reasonable default
- the measurement height is not derivable from the detector or geolocation, so `1.0 m` is treated as an editable heuristic default rather than a claimed measured fact

The existing OpenRadiation coordinates, altitude, and position accuracy stay unchanged.

## Error Handling

- Invalid environment value from the portal or backup import:
  - reject it and preserve the previous valid value
- Negative measurement height:
  - clamp to zero
- Missing or invalid queue-time timestamp:
  - keep the publish queued until time is valid and a fresh measurement window can be armed
- Missing or invalid pulse count:
  - omit `hitsNumber`
- Pulse counter reset or decreased value:
  - omit `hitsNumber`
- Missing environment or height:
  - submit the rest of the payload without them

## Testing

Follow test-first development for the new behavior.

Host-side coverage should include:

- config persistence for the new OpenRadiation fields
- backup JSON export/import for the new fields
- payload composition with:
  - configured environment and height
  - mirrored `altitudeAccuracy`
  - bounded `startTime` / `endTime`
  - valid `hitsNumber`
  - omitted `hitsNumber` on invalid or decreasing pulse counters
- portal handling for:
  - dropdown save/load
  - numeric measurement-height save/load
  - invalid environment rejection

Verification after implementation:

- run the relevant host tests
- run `pio run -e esp32-s3-devkitc-1 -t buildprog`
- run `pio run -e esp32-s3-devkitc-1 -t buildfs`
- OTA the firmware to `192.168.1.59`
- save the seeded live settings on the device
- verify the next OpenRadiation submission through the request API and confirm the new fields appear

## Non-Goals

- adding every optional OpenRadiation field
- building a separate OpenRadiation advanced-settings UI
- inferring weather or temperature from third-party services
- inventing smartphone metadata for the bridge
- guessing detector tube type across all supported devices
