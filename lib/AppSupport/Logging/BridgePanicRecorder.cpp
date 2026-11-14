// SPDX-License-Identifier: GPL-3.0-or-later
#include "Logging/BridgePanicRecorder.h"
#include "Logging/PanicCaptureBuffer.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <cstring>
namespace {
RTC_NOINIT_ATTR volatile PanicCaptureBuffer::State retained;
uint32_t bootId;
struct SavedRecord {
    uint32_t version, sequence, resetReason;
    PanicCaptureBuffer::State capture;
};
bool readRecord(Preferences &prefs, SavedRecord &out) {
    return prefs.getBytesLength("record") == sizeof(out) &&
           prefs.getBytes("record", &out, sizeof(out)) == sizeof(out) &&
           out.version == 1 && PanicCaptureBuffer::valid(out.capture);
}
void seedCurrentImage() {
    PanicCaptureBuffer::reset(retained);
    retained.bootId = bootId;
    const auto *desc = esp_ota_get_app_description();
    constexpr char hex[] = "0123456789abcdef";
    for (size_t n = 0; n < 32; ++n) {
        retained.elfSha256[n * 2] = hex[desc->app_elf_sha256[n] >> 4];
        retained.elfSha256[n * 2 + 1] = hex[desc->app_elf_sha256[n] & 15];
    }
    retained.elfSha256[64] = 0;
}
}
// Called from the actual panic output routine; no heap, locks, flash writes,
// clock reads or watchdog changes in the failing context.
extern "C" void IRAM_ATTR radpro_panic_capture_char(char c) {
    PanicCaptureBuffer::append(retained, c);
}
void BridgePanicRecorder::begin(Print &log) {
    bootId = esp_random();
    if (PanicCaptureBuffer::valid(retained) && retained.length) {
        Preferences prefs;
        bool saved = false;
        if (prefs.begin("panic-record", false)) {
            SavedRecord record{};
            const uint32_t sequence = readRecord(prefs, record) ? record.sequence + 1 : 1;
            record.version = 1;
            record.sequence = sequence;
            record.resetReason = esp_reset_reason();
            const volatile unsigned char *src = reinterpret_cast<const volatile unsigned char *>(&retained);
            auto *dst = reinterpret_cast<unsigned char *>(&record.capture);
            for (size_t n = 0; n < sizeof(retained); ++n) dst[n] = src[n];
            saved = prefs.putBytes("record", &record, sizeof(record)) == sizeof(record);
            prefs.end();
        }
        log.println(saved ? "Panic recorder: previous panic saved to flash." :
                            "Panic recorder: flash save failed; keeping retained panic for retry.");
        if (!saved) return; // Preserve evidence if NVS is temporarily unavailable.
    }
    seedCurrentImage();
    log.println("Panic recorder active; /diagnostics/crash.json retains the latest panic.");
}
String BridgePanicRecorder::json() {
    JsonDocument doc;
    doc["bootId"] = bootId;
    doc["resetReason"] = esp_reset_reason();
    doc["captureLimitBytes"] = PanicCaptureBuffer::kCapacity - 1;
    doc["retainedAcrossPowerLossBeforeSave"] = false;
    Preferences prefs;
    SavedRecord record{};
    bool available = false;
    if (prefs.begin("panic-record", true)) { available = readRecord(prefs, record); prefs.end(); }
    doc["available"] = available;
    if (available) {
        doc["sequence"] = record.sequence;
        doc["captureBootId"] = record.capture.bootId;
        doc["resetReasonAfterPanic"] = record.resetReason;
        record.capture.elfSha256[64] = 0;
        doc["elfSha256"] = record.capture.elfSha256;
        doc["bytes"] = record.capture.length;
        doc["truncated"] = bool(record.capture.truncated);
        doc["text"] = record.capture.text;
    }
    String body;
    serializeJson(doc, body);
    return body;
}
