# OpenRadiation Publishing

This guide explains how to connect the RadPro WiFi Bridge to [OpenRadiation](https://www.openradiation.org/) and what each OpenRadiation-specific field in the bridge portal does.

## 1. Get Your OpenRadiation Credentials

1. Create or log into your OpenRadiation account.
2. Request an API key for measurement uploads from the OpenRadiation team if you do not already have one.
3. Decide whether you want to provide a dedicated **Apparatus ID**:
   - Leave it blank to let the bridge reuse the connected detector's device ID automatically.
   - Fill it in if OpenRadiation assigned you a specific apparatus identifier for this station.

## 2. Configure the RadPro WiFi Bridge

1. Open the Wi-Fi portal and go to **Configure OpenRadiation**.
2. Tick **Enable OpenRadiation publishing**.
3. Enter:
   - **OpenRadiation Apparatus ID (optional)** - leave empty to use the detector device ID.
   - **OpenRadiation API Key** - the upload key issued by OpenRadiation.
   - **Latitude / Longitude** - station coordinates in decimal degrees.
   - **Altitude (m)** - station elevation in meters above sea level.
   - **Position accuracy (m)** - accuracy of the configured position.
   - **Measurement environment** - one of `countryside`, `city`, `inside`, `ontheroad`, or `plane`.
   - **Measurement height above ground (m)** - sensor height relative to the local ground level.
4. Save the form. Settings are written to NVS immediately.

If latitude or longitude is missing, the bridge will not publish to OpenRadiation.

## 3. Publishing Behaviour

- The bridge submits dose-rate measurements to `https://submit.openradiation.net/measurements`.
- Each publish includes the configured location metadata plus bridge-generated timestamps, software version, and a report UUID.
- The OpenRadiation page in the portal exposes:
  - a link to the public OpenRadiation map for the configured coordinates
  - a **Latest measurement** page that looks up the last successful report UUID via the OpenRadiation request API
- The bridge derives `altitudeAccuracy` from the configured position accuracy and includes `measurementEnvironment`, `measurementHeight`, `endTime`, and `hitsNumber` when valid data is available.

## 4. Verify Data

- Watch the serial console for `OpenRadiation: POST dose=...` messages.
- Open the bridge portal page at `/openradiation` and use the map link to confirm the station position.
- After the first successful upload, open `/openradiation/latest` to inspect the most recently published measurement.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `OpenRadiation: latitude/longitude not configured; skipping publish.` | Add valid coordinates in the portal and save again. |
| HTTP 4xx response from OpenRadiation | Re-check the API key, apparatus ID, and required location fields. |
| `/openradiation/latest` shows a 404 page | No successful OpenRadiation publish has been recorded yet. Wait for the first accepted upload. |
| Map link is missing on the portal page | Add latitude and longitude first; the map link is only shown when both are configured. |

Disable the feature if you do not use OpenRadiation; the firmware will not make OpenRadiation requests until the toggle is enabled and the required fields are present.
