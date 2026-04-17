# OpenRadiation Portal Links Design

Date: 2026-04-17
Branch: `OpenRadiation`

## Goal

Add OpenRadiation links to the bridge portal that mirror the OpenSenseMap experience:

- A primary link to the public OpenRadiation map.
- A secondary link to the latest successfully published OpenRadiation measurement.

The design must avoid exposing the user API key in browser URLs and must not depend on undocumented public URL patterns for a stable device page.

## Current Context

- OpenSenseMap already exposes contextual links from its configuration page through a dedicated helper (`OpenSenseMapPortalLinks`) and templated HTML.
- OpenRadiation currently uses inline HTML in `WiFiPortalService::sendOpenRadiationForm()` and has no external links.
- The current OpenRadiation integration already knows the configured coordinates and generates a `reportUuid` for each submission.
- Official OpenRadiation references document map permalinks and request-API lookup by `reportUuid`, but do not document a stable public per-device page keyed by `apparatusId`.

## Constraints

- Do not leak the OpenRadiation API key to the browser.
- Do not invent a public “device page” URL that is not documented.
- Preserve the current portal architecture and avoid broad refactors.
- Only show links when the underlying data needed to build them is available.

## Design

### 1. Primary Link: Public Map

Add an `OpenRadiationPortalLinks` helper, parallel to the OpenSenseMap helper.

Responsibilities:

- Build a public OpenRadiation map URL from configured latitude and longitude.
- Return an empty string when coordinates are missing or invalid.

Portal behavior:

- The OpenRadiation settings page renders `Open on OpenRadiation map` as the primary external link.
- The link only appears when non-zero coordinates are configured.

### 2. Secondary Link: Latest Published Measurement

Treat the “device” link as “latest successfully published measurement”.

Reasoning:

- OpenRadiation exposes measurement lookup by `reportUuid`.
- A stable public page by `apparatusId` was not found in the official docs reviewed for this feature.
- The latest published measurement is a real, verifiable target that matches the user intent without guessing at undocumented routes.

Implementation shape:

- Extend OpenRadiation publisher state with `lastPublishedReportUuid`.
- Update that field only after a successful OpenRadiation publish.
- Add a bridge route, `/openradiation/latest`, that performs the request-API lookup server-side using the stored `reportUuid`.
- Render a small HTML page with the latest measurement details and a link onward if the API returns a public OpenRadiation page URL later, or just render the measurement details directly if not.

This route keeps the API key on the device instead of in the browser.

### 3. Portal UX

Update the OpenRadiation configuration page to show:

- `Open on OpenRadiation map` as the primary external link.
- `Open latest published measurement` as the secondary link.
- A short note when the latest-measurement link is unavailable because no successful publish has happened yet.

The rest of the page remains structurally unchanged.

## Data Flow

1. User opens `/openradiation`.
2. Portal builds the map URL from configured coordinates.
3. Portal checks whether a successful OpenRadiation publish has recorded a `lastPublishedReportUuid`.
4. If present, the portal renders the secondary bridge-hosted link.
5. When the user opens `/openradiation/latest`, the bridge queries OpenRadiation by `reportUuid` server-side and renders the result.

## Error Handling

- Missing coordinates: omit the map link.
- No successful publish yet: omit the measurement link and show an explanatory note.
- Request API failure in `/openradiation/latest`: render an error page with the reason and keep the portal usable.
- Stale `reportUuid` or missing measurement: show a not-found style message rather than a broken redirect.

## Testing

- Add small host-side tests for the OpenRadiation portal-link helper.
- Verify the OpenRadiation settings page renders the map link only when coordinates exist.
- Verify the bridge route for latest measurement handles:
  - success,
  - missing `reportUuid`,
  - upstream fetch failure.
- Run the existing host test command for the new helper plus fresh PlatformIO `buildprog` and `buildfs`.

## Non-Goals

- Building a full OpenRadiation browsing UI in the bridge.
- Inventing or scraping undocumented public device-profile URLs.
- Changing the OpenRadiation publish payload or submission flow for this feature.

## Selected Approach

Use a documented public map permalink as the primary link and a bridge-hosted latest-measurement page as the secondary link.

This is the smallest design that matches the requested UX, stays inside documented API behavior, and avoids exposing credentials.
