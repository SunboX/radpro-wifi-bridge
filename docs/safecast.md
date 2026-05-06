# Safecast Publishing

This guide explains how to connect the RadPro WiFi Bridge to [Safecast](https://safecast.org/) and how the bridge's Safecast upload mode behaves.

## 1. Prepare Your Safecast Account

1. Create a Safecast account at [api.safecast.org/en-US/users/sign_up](https://api.safecast.org/en-US/users/sign_up) or log into an existing one.
2. Open your Safecast profile and copy your API key.
3. Decide whether you want to provide a **Device ID**:
   - Leave it blank if you only want to send the minimum required measurement fields.
   - Fill it in if you already created a Safecast device for this station and want uploads tied to that device record.
4. Decide which unit you want the bridge to publish:
   - `cpm` for count rate uploads.
   - `µSv/h` if your detector already reports dose rate and you want the bridge to send Safecast's internal `usv` unit value.

## 2. Configure the RadPro WiFi Bridge

1. Open the Wi-Fi portal and go to **Safecast konfigurieren**.
2. Tick **Enable Safecast publishing**.
3. Enter:
   - **API Endpoint** - normally `https://api.safecast.org`.
   - **API Key** - your Safecast API key from the profile page.
   - **Device ID** - optional numeric Safecast device ID.
   - **Latitude / Longitude** - station coordinates in decimal degrees.
   - **Height Above Ground (cm)** - optional sensor height relative to the local ground.
   - **Location Name / Label** - optional human-readable station label.
   - **Unit** - `cpm` or `µSv/h`.
   - **Upload Interval (seconds)** - minimum `60`, default `300`.
4. Optional advanced settings:
   - **Use development / test endpoint** - switches from the normal production host to the documented Safecast test host.
   - **Custom endpoint override** - use this if you want to point the bridge at a different Safecast-compatible host.
   - **Enable HTTP debug logging** - logs the redacted request URL, payload, status code, and response body to the serial console.
5. Save the form. Settings are written to NVS immediately.

The portal masks the stored API key when you reopen the page. Leave the API key field blank when editing other settings if you want to keep the saved key unchanged.

## Open Data Note

Safecast is an open-data platform. When you enable the integration, uploaded measurements, timestamps, and location data may become publicly visible and may be reused as open data.

Enable the feature only if you are comfortable with publishing that station metadata.

## 3. Test the Configuration

Use **Send Test Upload** before enabling unattended publishing for a station you care about.

- The bridge validates the current form values first.
- It uses the latest detector reading for the selected unit.
- It submits one measurement immediately.
- The page shows the HTTP status code and the response body.
- The test action does not save the form unless you also press **Save Safecast Settings**.

If the detector has not produced a reading for the chosen unit yet, the test upload will fail locally and no request is sent.

## 4. Publishing Behaviour

- The bridge publishes direct measurements to `POST /measurements.json`.
- Authentication is sent as `?api_key=<your key>` on the request URL.
- Payloads are JSON and include:
  - `latitude`
  - `longitude`
  - `value`
  - `unit`
  - `captured_at`
  - optional `device_id`
  - optional `height`
  - optional `location_name`
- Timestamps are generated in UTC ISO-8601 format.
- Automatic uploads are averaged over the configured upload interval instead of sending every detector tick.
- Negative or invalid detector values are ignored.
- The bridge keeps separate sample windows for CPM and dose rate, then chooses the correct one based on the configured Safecast unit.

### Unit Handling

- `cpm` in the portal is sent to Safecast as `cpm`.
- `µSv/h` in the portal is sent to Safecast as `usv`.
- The bridge does not invent a CPM-to-dose conversion. If you choose `µSv/h`, the detector must already provide a valid dose-rate reading.

## 5. Verify Data

- Watch the serial console for `Safecast: POST ...` messages or HTTP status logs.
- Use the bridge portal's test-upload result to confirm that Safecast accepts the current configuration.
- Check the public Safecast data view at [safecast.org/data](https://safecast.org/data/) after a successful production upload.

Safecast visibility may depend on their backend processing and the way the upload is associated with your account or device.

## Security Notes

- The bridge stores the Safecast API key in on-device NVS, not in the repository.
- The portal masks saved API keys when rendering the form.
- Serial logs redact the `api_key` query parameter before printing request URLs.
- Treat `/backup.json` exports as sensitive because they include saved service credentials.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `Safecast API key is required.` | Enter the API key and save again, or provide it in the test-upload form before testing. |
| `Latitude must be between -90 and 90.` or `Longitude must be between -180 and 180.` | Correct the station coordinates and save again. |
| `Upload interval must be at least 60 seconds.` | Raise the interval to `60` or more. |
| `No current detector reading is available for the selected unit.` | Wait for the detector to report CPM or dose rate for the chosen unit, then retry. |
| `Safecast: connect failed.` | Check Wi-Fi/DNS reachability and confirm the selected endpoint host is valid. |
| HTTP `401` or `403` | Re-check the API key and confirm the account is allowed to upload. |
| HTTP `404` | The endpoint URL is wrong. Use the default API host or a correct custom override. |
| HTTP `422` | Safecast rejected the payload fields. Re-check unit, coordinates, and optional numeric values such as device ID or height. |

If the documented Safecast development host is unavailable, leave **Use development / test endpoint** disabled and point **Custom endpoint override** at the test system you actually want to use.
