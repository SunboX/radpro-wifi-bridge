# OpenRadiation Missing Parameters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the OpenRadiation integration to publish `measurementEnvironment`, `measurementHeight`, `altitudeAccuracy`, `endTime`, and `hitsNumber`, expose the new settings in the portal, and verify the result on the live bridge at `192.168.1.59`.

**Architecture:** Keep the feature inside the existing OpenRadiation config and publisher flow, but pull the new validation, payload, and measurement-window logic into small helper headers that are cheap to host-test. Wire those helpers into `WiFiPortalService`, `AppConfigStore`, and `OpenRadiationPublisher`, then verify the full flow with fresh firmware and a live publish.

**Tech Stack:** C++17, Arduino/ESP32, ArduinoJson, PlatformIO, host-side `c++` tests, stdlib Python for OTA upload and live verification.

---

## File Structure

- Create: `lib/AppSupport/OpenRadiation/OpenRadiationMeasurementMetadata.h`
  - Responsibility: validate OpenRadiation measurement-environment values, clamp measurement height, parse pulse counters, compute `hitsNumber`, and append optional OpenRadiation metadata fields to a JSON payload.
- Create: `lib/AppSupport/OpenRadiation/OpenRadiationMeasurementWindow.h`
  - Responsibility: hold the queued OpenRadiation measurement window and provide pure helpers for arm/reset behavior.
- Create: `lib/AppSupport/OpenRadiation/OpenRadiationBackupJson.h`
  - Responsibility: append/apply the OpenRadiation-specific backup JSON fields so `WiFiPortalService` stays thin.
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPortalView.h`
  - Responsibility: render the dropdown `<option>` markup for the new measurement-environment field.
- Modify: `lib/AppSupport/AppConfig/AppConfig.h`
  - Responsibility: add persisted config fields for `openRadiationMeasurementEnvironment` and `openRadiationMeasurementHeight`.
- Modify: `lib/AppSupport/AppConfig/AppConfig.cpp`
  - Responsibility: load and save the new NVS fields.
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.h`
  - Responsibility: store the new measurement-window state.
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.cpp`
  - Responsibility: arm the window, send the expanded payload, and preserve/clear window state correctly.
- Modify: `lib/AppSupport/ConfigPortal/WiFiPortalService.cpp`
  - Responsibility: render/save the new portal controls and include the new OpenRadiation fields in backup export/import.
- Create: `test/host/include/Preferences.h`
  - Responsibility: host stub for `AppConfigStore` tests.
- Create: `test/host/openradiation_measurement_metadata_test.cpp`
  - Responsibility: host tests for metadata validation, `hitsNumber` math, and payload field injection.
- Create: `test/host/openradiation_measurement_window_test.cpp`
  - Responsibility: host tests for queue-window arm/reset behavior.
- Create: `test/host/openradiation_settings_persistence_test.cpp`
  - Responsibility: host tests for `AppConfigStore` persistence and backup JSON export/import of the new fields.
- Modify: `test/host/openradiation_portal_view_test.cpp`
  - Responsibility: host tests for dropdown rendering and selected-option behavior.

### Task 1: Add Host Test Scaffolding And Metadata Helper

**Files:**
- Create: `test/host/include/Preferences.h`
- Create: `test/host/openradiation_measurement_metadata_test.cpp`
- Create: `lib/AppSupport/OpenRadiation/OpenRadiationMeasurementMetadata.h`

- [ ] **Step 1: Write the failing metadata test**

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"

using OpenRadiationMeasurementMetadata::appendOptionalFields;
using OpenRadiationMeasurementMetadata::clampMeasurementHeight;
using OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment;
using OpenRadiationMeasurementMetadata::tryComputeHitsNumber;

namespace
{
void testAcceptsKnownMeasurementEnvironmentValues()
{
    assert(isValidMeasurementEnvironment("city"));
    assert(isValidMeasurementEnvironment("countryside"));
    assert(isValidMeasurementEnvironment("ontheroad"));
    assert(isValidMeasurementEnvironment("inside"));
    assert(isValidMeasurementEnvironment("plane"));
}

void testRejectsUnknownMeasurementEnvironmentValues()
{
    assert(!isValidMeasurementEnvironment(""));
    assert(!isValidMeasurementEnvironment("forest"));
    assert(!isValidMeasurementEnvironment("City"));
}

void testClampsMeasurementHeightToSupportedRange()
{
    assert(clampMeasurementHeight(-1.0f) == 0.0f);
    assert(clampMeasurementHeight(1.0f) == 1.0f);
    assert(clampMeasurementHeight(120.0f) == 100.0f);
}

void testComputesHitsNumberFromPulseDelta()
{
    uint32_t hits = 0;
    assert(tryComputeHitsNumber("100", "142", hits));
    assert(hits == 42);
    assert(!tryComputeHitsNumber("150", "142", hits));
    assert(!tryComputeHitsNumber("abc", "142", hits));
}

void testAppendsOptionalFieldsToPayload()
{
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "city";
    config.openRadiationMeasurementHeight = 1.0f;
    config.openRadiationAccuracy = 17.5f;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    appendOptionalFields(data, config, "2026-04-17T18:11:00Z", "100", "142");

    assert(std::string(data["measurementEnvironment"].as<const char *>()) == "city");
    assert(data["measurementHeight"].as<float>() == 1.0f);
    assert(data["altitudeAccuracy"].as<float>() == 17.5f);
    assert(std::string(data["endTime"].as<const char *>()) == "2026-04-17T18:11:00Z");
    assert(data["hitsNumber"].as<uint32_t>() == 42);
}
} // namespace

int main()
{
    testAcceptsKnownMeasurementEnvironmentValues();
    testRejectsUnknownMeasurementEnvironmentValues();
    testClampsMeasurementHeightToSupportedRange();
    testComputesHitsNumberFromPulseDelta();
    testAppendsOptionalFieldsToPayload();
    std::cout << "openradiation measurement metadata tests passed\n";
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_measurement_metadata_test.cpp \
  -o /tmp/openradiation_measurement_metadata_test
```

Expected: FAIL with a missing `OpenRadiation/OpenRadiationMeasurementMetadata.h` include or undefined symbols for the helper functions.

- [ ] **Step 3: Write the minimal helper and Preferences stub**

`test/host/include/Preferences.h`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "Arduino.h"

class Preferences
{
public:
    bool begin(const char *name, bool readOnly = false)
    {
        namespace_ = name ? name : "";
        readOnly_ = readOnly;
        return true;
    }

    void end() {}

    String getString(const char *key, const String &defaultValue = String()) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : String(it->second);
    }

    bool getBool(const char *key, bool defaultValue = false) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : it->second == "1";
    }

    uint16_t getUShort(const char *key, uint16_t defaultValue = 0) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : static_cast<uint16_t>(std::stoul(it->second));
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : static_cast<uint32_t>(std::stoul(it->second));
    }

    float getFloat(const char *key, float defaultValue = 0.0f) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : std::stof(it->second);
    }

    size_t putString(const char *key, const String &value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = value.c_str();
        return value.length();
    }

    size_t putBool(const char *key, bool value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = value ? "1" : "0";
        return 1;
    }

    size_t putUShort(const char *key, uint16_t value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

    size_t putUInt(const char *key, uint32_t value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

    size_t putFloat(const char *key, float value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

private:
    std::string scopedKey(const char *key) const
    {
        return namespace_ + ":" + (key ? key : "");
    }

    inline static std::unordered_map<std::string, std::string> store_{};
    std::string namespace_;
    bool readOnly_ = false;
};
```

`lib/AppSupport/OpenRadiation/OpenRadiationMeasurementMetadata.h`

```cpp
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "AppConfig/AppConfig.h"

namespace OpenRadiationMeasurementMetadata
{
inline bool isValidMeasurementEnvironment(const String &value)
{
    const char *raw = value.c_str();
    return raw &&
           (std::strcmp(raw, "countryside") == 0 ||
            std::strcmp(raw, "city") == 0 ||
            std::strcmp(raw, "ontheroad") == 0 ||
            std::strcmp(raw, "inside") == 0 ||
            std::strcmp(raw, "plane") == 0);
}

inline float clampMeasurementHeight(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    return value;
}

inline bool tryParsePulseCount(const String &value, uint32_t &out)
{
    const char *raw = value.c_str();
    if (!raw || !*raw)
        return false;
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (!end || *end != '\0')
        return false;
    out = static_cast<uint32_t>(parsed);
    return true;
}

inline bool tryComputeHitsNumber(const String &startValue, const String &endValue, uint32_t &out)
{
    uint32_t startPulseCount = 0;
    uint32_t endPulseCount = 0;
    if (!tryParsePulseCount(startValue, startPulseCount))
        return false;
    if (!tryParsePulseCount(endValue, endPulseCount))
        return false;
    if (endPulseCount < startPulseCount)
        return false;
    out = endPulseCount - startPulseCount;
    return true;
}

inline void appendOptionalFields(JsonObject data,
                                 const AppConfig &config,
                                 const String &endTime,
                                 const String &startPulseCount,
                                 const String &endPulseCount)
{
    if (config.openRadiationMeasurementEnvironment.length() &&
        isValidMeasurementEnvironment(config.openRadiationMeasurementEnvironment))
        data["measurementEnvironment"] = config.openRadiationMeasurementEnvironment;

    if (config.openRadiationMeasurementHeight > 0.0f)
        data["measurementHeight"] = clampMeasurementHeight(config.openRadiationMeasurementHeight);

    if (config.openRadiationAccuracy > 0.0f)
        data["altitudeAccuracy"] = config.openRadiationAccuracy;

    if (endTime.length())
        data["endTime"] = endTime;

    uint32_t hitsNumber = 0;
    if (tryComputeHitsNumber(startPulseCount, endPulseCount, hitsNumber))
        data["hitsNumber"] = hitsNumber;
}
} // namespace OpenRadiationMeasurementMetadata
```

- [ ] **Step 4: Run the metadata test to verify it passes**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_measurement_metadata_test.cpp \
  -o /tmp/openradiation_measurement_metadata_test && \
  /tmp/openradiation_measurement_metadata_test
```

Expected: PASS with `openradiation measurement metadata tests passed`.

- [ ] **Step 5: Commit**

```bash
git add \
  test/host/include/Preferences.h \
  test/host/openradiation_measurement_metadata_test.cpp \
  lib/AppSupport/OpenRadiation/OpenRadiationMeasurementMetadata.h
git commit -m "test: add OpenRadiation metadata helpers"
```

### Task 2: Add The Portal Dropdown Renderer

**Files:**
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPortalView.h`
- Modify: `test/host/openradiation_portal_view_test.cpp`

- [ ] **Step 1: Extend the portal-view test with a failing dropdown assertion**

```cpp
void testMeasurementEnvironmentOptionsMarkSelectedValue()
{
    const std::string html = buildMeasurementEnvironmentOptions("city");
    assert(html.find("<option value='city' selected>City</option>") != std::string::npos);
    assert(html.find("<option value='countryside'>Countryside</option>") != std::string::npos);
    assert(html.find("<option value='inside'>Inside</option>") != std::string::npos);
}
```

Add the call in `main()`:

```cpp
    testMeasurementEnvironmentOptionsMarkSelectedValue();
```

- [ ] **Step 2: Run the portal-view test to verify it fails**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_portal_view_test.cpp \
  -o /tmp/openradiation_portal_view_test
```

Expected: FAIL with `buildMeasurementEnvironmentOptions` not declared.

- [ ] **Step 3: Implement the dropdown renderer**

Add to `lib/AppSupport/OpenRadiation/OpenRadiationPortalView.h`:

```cpp
inline String buildMeasurementEnvironmentOptions(const String &selectedValue)
{
    struct Option
    {
        const char *value;
        const char *label;
    };

    static constexpr Option kOptions[] = {
        {"countryside", "Countryside"},
        {"city", "City"},
        {"ontheroad", "On the road"},
        {"inside", "Inside"},
        {"plane", "Plane"},
    };

    String html;
    for (const auto &option : kOptions)
    {
        html += "<option value='";
        html += option.value;
        html += "'";
        if (std::strcmp(selectedValue.c_str(), option.value) == 0)
            html += " selected";
        html += ">";
        html += option.label;
        html += "</option>";
    }
    return html;
}
```

- [ ] **Step 4: Re-run the portal-view test to verify it passes**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_portal_view_test.cpp \
  -o /tmp/openradiation_portal_view_test && \
  /tmp/openradiation_portal_view_test
```

Expected: PASS with `openradiation portal view tests passed`.

- [ ] **Step 5: Commit**

```bash
git add \
  lib/AppSupport/OpenRadiation/OpenRadiationPortalView.h \
  test/host/openradiation_portal_view_test.cpp
git commit -m "test: cover OpenRadiation environment dropdown"
```

### Task 3: Persist The New Settings And Cover Backup JSON

**Files:**
- Modify: `lib/AppSupport/AppConfig/AppConfig.h`
- Modify: `lib/AppSupport/AppConfig/AppConfig.cpp`
- Create: `lib/AppSupport/OpenRadiation/OpenRadiationBackupJson.h`
- Create: `test/host/openradiation_settings_persistence_test.cpp`

- [ ] **Step 1: Write the failing persistence and backup test**

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationBackupJson.h"

namespace
{
void testAppConfigStorePersistsNewMeasurementFields()
{
    AppConfigStore store;
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "city";
    config.openRadiationMeasurementHeight = 1.0f;
    assert(store.save(config));

    AppConfig loaded;
    assert(store.load(loaded));
    assert(std::string(loaded.openRadiationMeasurementEnvironment.c_str()) == "city");
    assert(loaded.openRadiationMeasurementHeight == 1.0f);
}

void testBackupJsonRoundTripsNewMeasurementFields()
{
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "inside";
    config.openRadiationMeasurementHeight = 2.5f;

    JsonDocument doc;
    OpenRadiationBackupJson::appendMeasurementConfig(doc, config);
    assert(std::string(doc["openRadiationMeasurementEnvironment"].as<const char *>()) == "inside");
    assert(doc["openRadiationMeasurementHeight"].as<float>() == 2.5f);

    AppConfig updated;
    JsonDocument input;
    input["openRadiationMeasurementEnvironment"] = "city";
    input["openRadiationMeasurementHeight"] = -7.0f;
    OpenRadiationBackupJson::applyMeasurementConfig(input.as<JsonVariantConst>(), updated);

    assert(std::string(updated.openRadiationMeasurementEnvironment.c_str()) == "city");
    assert(updated.openRadiationMeasurementHeight == 0.0f);
}

void testBackupJsonRejectsInvalidEnvironmentAndKeepsPreviousValue()
{
    AppConfig updated;
    updated.openRadiationMeasurementEnvironment = "city";

    JsonDocument input;
    input["openRadiationMeasurementEnvironment"] = "forest";
    OpenRadiationBackupJson::applyMeasurementConfig(input.as<JsonVariantConst>(), updated);

    assert(std::string(updated.openRadiationMeasurementEnvironment.c_str()) == "city");
}
} // namespace

int main()
{
    testAppConfigStorePersistsNewMeasurementFields();
    testBackupJsonRoundTripsNewMeasurementFields();
    testBackupJsonRejectsInvalidEnvironmentAndKeepsPreviousValue();
    std::cout << "openradiation settings persistence tests passed\n";
    return 0;
}
```

- [ ] **Step 2: Run the persistence test to verify it fails**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_settings_persistence_test.cpp \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  -o /tmp/openradiation_settings_persistence_test
```

Expected: FAIL because the new config fields and `OpenRadiationBackupJson` helper do not exist yet.

- [ ] **Step 3: Implement the new config fields and backup helper**

Add to `lib/AppSupport/AppConfig/AppConfig.h` inside `struct AppConfig`:

```cpp
    String openRadiationMeasurementEnvironment;
    float openRadiationMeasurementHeight = 0.0f;
```

Add to `lib/AppSupport/AppConfig/AppConfig.cpp` load/save blocks:

```cpp
    cfg.openRadiationMeasurementEnvironment = prefs_.getString("orEnv", cfg.openRadiationMeasurementEnvironment);
    cfg.openRadiationMeasurementEnvironment.trim();
    cfg.openRadiationMeasurementHeight = prefs_.getFloat("orHeight", cfg.openRadiationMeasurementHeight);
```

```cpp
    prefs_.putString("orEnv", cfg.openRadiationMeasurementEnvironment);
    prefs_.putFloat("orHeight", cfg.openRadiationMeasurementHeight);
```

Create `lib/AppSupport/OpenRadiation/OpenRadiationBackupJson.h`:

```cpp
#pragma once

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"

namespace OpenRadiationBackupJson
{
inline void appendMeasurementConfig(JsonDocument &doc, const AppConfig &config)
{
    doc["openRadiationMeasurementEnvironment"] = config.openRadiationMeasurementEnvironment;
    doc["openRadiationMeasurementHeight"] = config.openRadiationMeasurementHeight;
}

inline void applyMeasurementConfig(JsonVariantConst root, AppConfig &config)
{
    if (!root["openRadiationMeasurementEnvironment"].isNull())
    {
        String environment = root["openRadiationMeasurementEnvironment"].as<String>();
        environment.trim();
        if (!environment.length())
        {
            config.openRadiationMeasurementEnvironment = String();
        }
        else if (OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment(environment))
        {
            config.openRadiationMeasurementEnvironment = environment;
        }
    }

    if (!root["openRadiationMeasurementHeight"].isNull())
    {
        config.openRadiationMeasurementHeight = OpenRadiationMeasurementMetadata::clampMeasurementHeight(
            root["openRadiationMeasurementHeight"].as<float>());
    }
}
} // namespace OpenRadiationBackupJson
```

- [ ] **Step 4: Re-run the persistence test to verify it passes**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_settings_persistence_test.cpp \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  -o /tmp/openradiation_settings_persistence_test && \
  /tmp/openradiation_settings_persistence_test
```

Expected: PASS with `openradiation settings persistence tests passed`.

- [ ] **Step 5: Commit**

```bash
git add \
  lib/AppSupport/AppConfig/AppConfig.h \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  lib/AppSupport/OpenRadiation/OpenRadiationBackupJson.h \
  test/host/openradiation_settings_persistence_test.cpp
git commit -m "feat: persist OpenRadiation measurement settings"
```

### Task 4: Add Measurement-Window Helpers And Wire The Publisher

**Files:**
- Create: `lib/AppSupport/OpenRadiation/OpenRadiationMeasurementWindow.h`
- Create: `test/host/openradiation_measurement_window_test.cpp`
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.h`
- Modify: `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.cpp`

- [ ] **Step 1: Write the failing measurement-window test**

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationMeasurementWindow.h"

using OpenRadiationMeasurementWindow::MeasurementWindowState;
using OpenRadiationMeasurementWindow::armMeasurementWindow;
using OpenRadiationMeasurementWindow::clearMeasurementWindow;
using OpenRadiationMeasurementWindow::hasMeasurementWindow;

namespace
{
void testArmCapturesOnlyTheFirstQueuedWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");
    armMeasurementWindow(state, "2026-04-17T18:11:00Z", "142");

    assert(std::string(state.startTime.c_str()) == "2026-04-17T18:10:00Z");
    assert(std::string(state.startPulseCount.c_str()) == "100");
}

void testClearResetsTheWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");
    clearMeasurementWindow(state);

    assert(!hasMeasurementWindow(state));
    assert(std::string(state.startPulseCount.c_str()).empty());
}

void testEmptyTimestampDoesNotArmTheWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "", "100");
    assert(!hasMeasurementWindow(state));
}
} // namespace

int main()
{
    testArmCapturesOnlyTheFirstQueuedWindow();
    testClearResetsTheWindow();
    testEmptyTimestampDoesNotArmTheWindow();
    std::cout << "openradiation measurement window tests passed\n";
    return 0;
}
```

- [ ] **Step 2: Run the measurement-window test to verify it fails**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_measurement_window_test.cpp \
  -o /tmp/openradiation_measurement_window_test
```

Expected: FAIL with a missing `OpenRadiation/OpenRadiationMeasurementWindow.h` include or undefined symbols.

- [ ] **Step 3: Implement the helper and wire the publisher**

Create `lib/AppSupport/OpenRadiation/OpenRadiationMeasurementWindow.h`:

```cpp
#pragma once

#include <Arduino.h>

namespace OpenRadiationMeasurementWindow
{
struct MeasurementWindowState
{
    String startTime;
    String startPulseCount;
};

inline bool hasMeasurementWindow(const MeasurementWindowState &state)
{
    return state.startTime.length() > 0;
}

inline void armMeasurementWindow(MeasurementWindowState &state,
                                 const String &startTime,
                                 const String &startPulseCount)
{
    if (state.startTime.length() || !startTime.length())
        return;
    state.startTime = startTime;
    state.startPulseCount = startPulseCount;
}

inline void clearMeasurementWindow(MeasurementWindowState &state)
{
    state.startTime = String();
    state.startPulseCount = String();
}
} // namespace OpenRadiationMeasurementWindow
```

Add to `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.h`:

```cpp
#include "OpenRadiation/OpenRadiationMeasurementWindow.h"
```

```cpp
    bool buildPayload(String &outJson,
                      String &outReportUuid,
                      float doseRate,
                      const String &startTime,
                      const String &endTime,
                      const String &startPulseCount,
                      const String &endPulseCount);
```

```cpp
    OpenRadiationMeasurementWindow::MeasurementWindowState measurementWindow_;
```

Update `lib/AppSupport/OpenRadiation/OpenRadiationPublisher.cpp` includes:

```cpp
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"
#include "OpenRadiation/OpenRadiationMeasurementWindow.h"
```

Update `clearPendingData()`:

```cpp
    OpenRadiationMeasurementWindow::clearMeasurementWindow(measurementWindow_);
```

Update `onCommandResult()` when the publish is queued:

```cpp
            if (haveTubeValue_)
            {
                publishQueued_ = true;
                suppressUntilMs_ = 0;

                String queuedTimestamp;
                if (makeIsoTimestamp(queuedTimestamp))
                {
                    const DeviceInfoSnapshot info = deviceInfo_.snapshot();
                    OpenRadiationMeasurementWindow::armMeasurementWindow(
                        measurementWindow_,
                        queuedTimestamp,
                        info.tubePulseCount);
                }
            }
```

Update `publishPending()` after `makeIsoTimestamp(endTimestamp)` succeeds:

```cpp
    const DeviceInfoSnapshot info = deviceInfo_.snapshot();
    if (!OpenRadiationMeasurementWindow::hasMeasurementWindow(measurementWindow_))
    {
        OpenRadiationMeasurementWindow::armMeasurementWindow(
            measurementWindow_,
            endTimestamp,
            info.tubePulseCount);
    }
```

Update the `buildPayload()` call:

```cpp
    if (!buildPayload(payload,
                      reportUuid,
                      doseRate,
                      measurementWindow_.startTime,
                      endTimestamp,
                      measurementWindow_.startPulseCount,
                      info.tubePulseCount))
```

Update the success and invalid-dose reset paths:

```cpp
        OpenRadiationMeasurementWindow::clearMeasurementWindow(measurementWindow_);
```

Update `buildPayload()` to use the helper:

```cpp
bool OpenRadiationPublisher::buildPayload(String &outJson,
                                          String &outReportUuid,
                                          float doseRate,
                                          const String &startTime,
                                          const String &endTime,
                                          const String &startPulseCount,
                                          const String &endPulseCount)
{
    const String apparatusId = resolveApparatusId();
    if (!apparatusId.length() || !config_.openRadiationApiKey.length() || !startTime.length())
        return false;

    outReportUuid = generateUuid();

    const DeviceInfoSnapshot info = deviceInfo_.snapshot();

    JsonDocument doc;
    doc["apiKey"] = config_.openRadiationApiKey;

    JsonObject data = doc["data"].to<JsonObject>();
    data["apparatusId"] = apparatusId;
    data["value"] = doseRate;
    data["startTime"] = startTime;
    data["latitude"] = config_.openRadiationLatitude;
    data["longitude"] = config_.openRadiationLongitude;
    if (config_.openRadiationAltitude != 0.0f)
        data["altitude"] = lroundf(config_.openRadiationAltitude);
    if (config_.openRadiationAccuracy > 0.0f)
        data["accuracy"] = config_.openRadiationAccuracy;
    if (info.firmware.length())
        data["apparatusVersion"] = info.firmware;
    data["apparatusSensorType"] = "geiger";
    data["reportUuid"] = outReportUuid;
    data["manualReporting"] = false;
    data["organisationReporting"] = OpenRadiationProtocol::buildOrganisationReporting(bridgeVersion_);
    data["reportContext"] = "routine";

    OpenRadiationMeasurementMetadata::appendOptionalFields(
        data,
        config_,
        endTime,
        startPulseCount,
        endPulseCount);

    outJson.clear();
    serializeJson(doc, outJson);
    return true;
}
```

- [ ] **Step 4: Run the new host test and a firmware build to verify the wiring**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_measurement_window_test.cpp \
  -o /tmp/openradiation_measurement_window_test && \
  /tmp/openradiation_measurement_window_test
```

Expected: PASS with `openradiation measurement window tests passed`.

Run:

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t buildprog
```

Expected: PASS with `SUCCESS` and a rebuilt `.pio/build/esp32-s3-devkitc-1/firmware.bin`.

- [ ] **Step 5: Commit**

```bash
git add \
  lib/AppSupport/OpenRadiation/OpenRadiationMeasurementWindow.h \
  lib/AppSupport/OpenRadiation/OpenRadiationPublisher.h \
  lib/AppSupport/OpenRadiation/OpenRadiationPublisher.cpp \
  test/host/openradiation_measurement_window_test.cpp
git commit -m "feat: publish OpenRadiation measurement window metadata"
```

### Task 5: Wire The Portal Form And Backup JSON Into WiFiPortalService

**Files:**
- Modify: `lib/AppSupport/ConfigPortal/WiFiPortalService.cpp`

- [ ] **Step 1: Update the OpenRadiation form and POST handling**

Add includes near the top of `lib/AppSupport/ConfigPortal/WiFiPortalService.cpp`:

```cpp
#include "OpenRadiation/OpenRadiationBackupJson.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"
#include "OpenRadiation/OpenRadiationPortalView.h"
```

Update the OpenRadiation page style block to cover `<select>`:

```cpp
            "<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:24px;display:flex;justify-content:center;}"
            "h1{margin-top:0;}form{display:flex;flex-direction:column;gap:12px;width:100%;}"
            "label{font-weight:bold;}input,select{padding:8px;border-radius:4px;border:1px solid #666;background:#222;color:#eee;width:100%;}"
            "button{padding:10px;border:none;border-radius:4px;background:#2196F3;color:#fff;font-size:15px;cursor:pointer;width:100%;}"
```

Add the new controls in `sendOpenRadiationForm()` after the accuracy field:

```cpp
    html += F("<label for='orMeasurementEnvironment'>Measurement Environment</label><select id='orMeasurementEnvironment' name='orMeasurementEnvironment'>");
    html += OpenRadiationPortalView::buildMeasurementEnvironmentOptions(config_.openRadiationMeasurementEnvironment);
    html += F("</select>");

    html += F("<label for='orMeasurementHeight'>Measurement Height Above Ground (m, optional)</label><input id='orMeasurementHeight' name='orMeasurementHeight' type='number' step='0.1' min='0' max='100' value='");
    html += String(config_.openRadiationMeasurementHeight, 1);
    html += F("'/>");
```

Add new POST args to `handleOpenRadiationPost()`:

```cpp
    String environmentStr = server.arg("orMeasurementEnvironment");
    String measurementHeightStr = server.arg("orMeasurementHeight");
```

```cpp
    environmentStr.trim();
    measurementHeightStr.trim();
```

Add validation and save logic:

```cpp
    if (!environmentStr.length())
    {
        changed |= UpdateStringIfChanged(config_.openRadiationMeasurementEnvironment, "");
    }
    else if (OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment(environmentStr))
    {
        changed |= UpdateStringIfChanged(config_.openRadiationMeasurementEnvironment, environmentStr.c_str());
    }
    else
    {
        log_.println("OpenRadiation: invalid measurementEnvironment ignored.");
    }

    float parsedMeasurementHeight = 0.0f;
    if (measurementHeightStr.length())
        parsedMeasurementHeight = OpenRadiationMeasurementMetadata::clampMeasurementHeight(strtof(measurementHeightStr.c_str(), nullptr));
    if (fabsf(parsedMeasurementHeight - config_.openRadiationMeasurementHeight) > 0.05f)
    {
        config_.openRadiationMeasurementHeight = parsedMeasurementHeight;
        changed = true;
    }
```

Update backup export/import:

```cpp
    OpenRadiationBackupJson::appendMeasurementConfig(doc, config_);
```

```cpp
    OpenRadiationBackupJson::applyMeasurementConfig(doc.as<JsonVariantConst>(), updated);
```

- [ ] **Step 2: Run the host tests that cover the reused helpers**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_portal_view_test.cpp \
  -o /tmp/openradiation_portal_view_test && \
  /tmp/openradiation_portal_view_test
```

Expected: PASS with `openradiation portal view tests passed`.

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_settings_persistence_test.cpp \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  -o /tmp/openradiation_settings_persistence_test && \
  /tmp/openradiation_settings_persistence_test
```

Expected: PASS with `openradiation settings persistence tests passed`.

- [ ] **Step 3: Run both firmware builds to verify the portal wiring**

Run:

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t buildprog
~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t buildfs
```

Expected: both commands finish with `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add lib/AppSupport/ConfigPortal/WiFiPortalService.cpp
git commit -m "feat: add OpenRadiation measurement fields to portal"
```

### Task 6: Run Full Verification On The Live Bridge

**Files:**
- Modify: none
- Verify: `docs/web-install/firmware/firmware_*.zip`
- Verify: live bridge `http://192.168.1.59`

- [ ] **Step 1: Re-run the full local verification set**

Run:

```bash
c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_measurement_metadata_test.cpp \
  -o /tmp/openradiation_measurement_metadata_test && \
  /tmp/openradiation_measurement_metadata_test

c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_measurement_window_test.cpp \
  -o /tmp/openradiation_measurement_window_test && \
  /tmp/openradiation_measurement_window_test

c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  test/host/openradiation_portal_view_test.cpp \
  -o /tmp/openradiation_portal_view_test && \
  /tmp/openradiation_portal_view_test

c++ -std=c++17 \
  -Itest/host/include \
  -Ilib/AppSupport \
  -I.pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src \
  test/host/openradiation_settings_persistence_test.cpp \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  -o /tmp/openradiation_settings_persistence_test && \
  /tmp/openradiation_settings_persistence_test

~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t buildprog
~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t buildfs
python3 tools/create_ota_zip.py
```

Expected: all host tests print `... tests passed`, both PlatformIO builds finish with `SUCCESS`, and the OTA ZIP generator emits a fresh `docs/web-install/firmware/firmware_<version>.zip`.

- [ ] **Step 2: OTA-upload the generated ZIP to the bridge**

Run:

```bash
python3 - <<'PY'
import base64
import json
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

bridge = "http://192.168.1.59"
zip_path = max(Path("docs/web-install/firmware").glob("firmware_*.zip"), key=lambda path: path.stat().st_mtime)

def post(url: str, body: bytes = b"", content_type: str = "application/json") -> None:
    req = urllib.request.Request(url, data=body, method="POST", headers={"Content-Type": content_type})
    with urllib.request.urlopen(req) as response:
        print(url, response.read().decode("utf-8"))

with zipfile.ZipFile(zip_path) as archive:
    manifest = archive.read("manifest.json")
    manifest_json = json.loads(manifest.decode("utf-8"))
    post(f"{bridge}/ota/upload/begin", manifest, "text/plain")

    for part in manifest_json["builds"][0]["parts"]:
        payload = archive.read(part["path"])
        query = urllib.parse.urlencode(
            {"path": part["path"], "offset": part.get("offset", 0), "size": len(payload)}
        )
        post(f"{bridge}/ota/upload/part/begin?{query}")
        for offset in range(0, len(payload), 16384):
            chunk = base64.b64encode(payload[offset : offset + 16384])
            post(f"{bridge}/ota/upload/part/chunk", chunk, "text/plain")
        finish_query = urllib.parse.urlencode({"path": part["path"]})
        post(f"{bridge}/ota/upload/part/finish?{finish_query}")

    post(f"{bridge}/ota/upload/finish")
PY
```

Expected: every `/ota/upload/*` call returns `{"ok":true...}` and the bridge reboots after `/ota/upload/finish`.

- [ ] **Step 3: Seed the live OpenRadiation config with the new fields**

Run:

```bash
curl -fsS -X POST http://192.168.1.59/openradiation \
  --data-urlencode 'orEnabled=1' \
  --data-urlencode 'orDeviceId=' \
  --data-urlencode 'orApiKey=9zrt01lmkkkk78hoc442xwg22l1rq232' \
  --data-urlencode 'orLatitude=50.497448' \
  --data-urlencode 'orLongitude=12.138785' \
  --data-urlencode 'orAltitude=361.0' \
  --data-urlencode 'orAccuracy=17.5' \
  --data-urlencode 'orMeasurementEnvironment=city' \
  --data-urlencode 'orMeasurementHeight=1.0'
```

Expected: the HTML response includes `OpenRadiation settings saved.` and the form echoes `city` and `1.0`.

- [ ] **Step 4: Verify backup export and live publish fields**

Run:

```bash
python3 - <<'PY'
import json
import time
import urllib.request

bridge = "http://192.168.1.59"
api_key = "9zrt01lmkkkk78hoc442xwg22l1rq232"

backup = json.load(urllib.request.urlopen(f"{bridge}/backup.json"))
assert backup["openRadiationMeasurementEnvironment"] == "city"
assert abs(backup["openRadiationMeasurementHeight"] - 1.0) < 0.001
print("backup ok")

report_uuid = ""
for _ in range(24):
    bridge_json = json.load(urllib.request.urlopen(f"{bridge}/bridge.json"))
    report_uuid = bridge_json["publishers"]["openRadiation"]["lastReportUuid"] or ""
    if report_uuid:
        break
    time.sleep(5)

if not report_uuid:
    raise SystemExit("No OpenRadiation publish recorded within 120 seconds")

measurement = json.load(
    urllib.request.urlopen(
        f"https://request.openradiation.net/measurements/{report_uuid}?apiKey={api_key}&response=complete&withEnclosedObject=no"
    )
)["data"]

assert measurement["measurementEnvironment"] == "city"
assert abs(measurement["measurementHeight"] - 1.0) < 0.001
assert abs(measurement["altitudeAccuracy"] - 17.5) < 0.001
assert "endTime" in measurement and measurement["endTime"]
assert "hitsNumber" in measurement
print(json.dumps({
    "reportUuid": report_uuid,
    "measurementEnvironment": measurement["measurementEnvironment"],
    "measurementHeight": measurement["measurementHeight"],
    "altitudeAccuracy": measurement["altitudeAccuracy"],
    "endTime": measurement["endTime"],
    "hitsNumber": measurement["hitsNumber"],
}, indent=2))
PY
```

Expected: `backup ok` followed by a JSON object showing the live OpenRadiation measurement contains `measurementEnvironment`, `measurementHeight`, `altitudeAccuracy`, `endTime`, and `hitsNumber`.

- [ ] **Step 5: Commit**

```bash
git status --short
git add \
  lib/AppSupport/AppConfig/AppConfig.h \
  lib/AppSupport/AppConfig/AppConfig.cpp \
  lib/AppSupport/ConfigPortal/WiFiPortalService.cpp \
  lib/AppSupport/OpenRadiation/OpenRadiationBackupJson.h \
  lib/AppSupport/OpenRadiation/OpenRadiationMeasurementMetadata.h \
  lib/AppSupport/OpenRadiation/OpenRadiationMeasurementWindow.h \
  lib/AppSupport/OpenRadiation/OpenRadiationPortalView.h \
  lib/AppSupport/OpenRadiation/OpenRadiationPublisher.h \
  lib/AppSupport/OpenRadiation/OpenRadiationPublisher.cpp \
  test/host/include/Preferences.h \
  test/host/openradiation_measurement_metadata_test.cpp \
  test/host/openradiation_measurement_window_test.cpp \
  test/host/openradiation_portal_view_test.cpp \
  test/host/openradiation_settings_persistence_test.cpp
git commit -m "feat: publish richer OpenRadiation measurement metadata"
```

## Self-Review

- Spec coverage:
  - `measurementEnvironment` dropdown and `measurementHeight` input are covered by Tasks 2, 3, and 5.
  - `altitudeAccuracy`, `endTime`, and `hitsNumber` are covered by Tasks 1 and 4.
  - backup export/import coverage is handled by Task 3 and live verification in Task 6.
  - live bridge seeding and OpenRadiation response verification are covered by Task 6.
- Placeholder scan:
  - no `TBD`, `TODO`, or “similar to Task N” references remain.
  - every code-changing step includes concrete code.
- Type consistency:
  - the plan consistently uses `openRadiationMeasurementEnvironment`, `openRadiationMeasurementHeight`, `orMeasurementEnvironment`, and `orMeasurementHeight`.
  - the payload helper and publisher both use `String` pulse-counter values and compute `hitsNumber` via the same helper.
