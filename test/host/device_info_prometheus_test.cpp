// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "DeviceInfoPage.h"

namespace
{
using Type = DeviceManager::CommandType;

void expect(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void populate(DeviceInfoStore &store)
{
    store.setBridgeFirmware("1.15.13");
    store.update(Type::DeviceModel, "FNIRSI GC-01");
    store.update(Type::DeviceFirmware, "Rad Pro 3.1");
    store.update(Type::DeviceId, "gc01-123456");
    store.update(Type::DeviceLocale, "en");
    store.update(Type::DevicePower, "1");
    store.update(Type::DeviceBatteryVoltage, "4.100");
    store.update(Type::DeviceBatteryPercent, "92");
    store.update(Type::TubeRate, "23.0");
    store.update(Type::TubeDoseRate, "0.14954");
    store.update(Type::TubePulseCount, "4294967296");
}

std::string scrape(DeviceInfoStore &store)
{
    WebServer server;
    WiFiManager manager{&server};
    DeviceInfoPage page(store);
    page.handlePrometheus(&manager);
    expect(server.status_ == 200, "metrics response should be HTTP 200");
    expect(server.contentType_ == "text/plain", "metrics should use the text format");
    return server.body_.c_str();
}

const std::vector<std::string> numericNames = {
    "radpro_tube_rate", "radpro_tube_dose_rate", "radpro_tube_pulse_count",
    "radpro_battery_voltage", "radpro_battery_percent"};

void expectNoSample(const std::string &body, const std::string &name)
{
    expect(body.find("\n" + name + " ") == std::string::npos,
           "unavailable measurements must be omitted");
}

void testBootOmitsUnavailableReadings()
{
    DeviceInfoStore store;
    const auto body = scrape(store);
    expect(body.find("radpro_info{") != std::string::npos,
           "device info must remain available before measurements");
    for (const auto &name : numericNames)
        expectNoSample(body, name);
}

void testDisconnectOmitsAllLiveReadings()
{
    DeviceInfoStore store;
    populate(store);
    store.clearLiveData();
    const auto body = scrape(store);
    for (const auto &name : numericNames)
        expectNoSample(body, name);
}

void testClearedMeasurementsPreserveBatteryReadings()
{
    DeviceInfoStore store;
    populate(store);
    store.clearMeasurements();
    const auto body = scrape(store);
    expectNoSample(body, "radpro_tube_rate");
    expectNoSample(body, "radpro_tube_dose_rate");
    expectNoSample(body, "radpro_tube_pulse_count");
    expect(body.find("\nradpro_battery_voltage 4.100\n") != std::string::npos,
           "clearing measurements must preserve available battery voltage");
    expect(body.find("\nradpro_battery_percent 92\n") != std::string::npos,
           "clearing measurements must preserve available battery percentage");
}

void testEachMissingReadingIsOmittedIndependently()
{
    const Type types[] = {Type::TubeRate, Type::TubeDoseRate, Type::TubePulseCount,
                          Type::DeviceBatteryVoltage, Type::DeviceBatteryPercent};
    for (size_t i = 0; i < numericNames.size(); ++i)
    {
        DeviceInfoStore store;
        populate(store);
        store.update(types[i], "");
        const auto body = scrape(store);
        expectNoSample(body, numericNames[i]);
        for (size_t j = 0; j < numericNames.size(); ++j)
            if (i != j)
                expect(body.find("\n" + numericNames[j] + " ") != std::string::npos,
                       "one missing reading must not remove another reading");
    }
}

void testCompleteReadingsRetainPrecision()
{
    DeviceInfoStore store;
    populate(store);
    const auto body = scrape(store);
    expect(body.find("\nradpro_tube_rate 23.0\n") != std::string::npos,
           "CPM must retain its original precision");
    expect(body.find("\nradpro_tube_dose_rate 0.14954\n") != std::string::npos,
           "dose rate must retain its original precision");
    expect(body.find("\nradpro_tube_pulse_count 4294967296\n") != std::string::npos,
           "pulse count must not be narrowed to 32 bits");
    expect(!body.empty() && body.back() == '\n', "response must end in a line feed");
}

void testZeroReadingsRemainAvailable()
{
    DeviceInfoStore store;
    store.update(Type::TubeRate, "0");
    store.update(Type::TubeDoseRate, "0.00000");
    store.update(Type::TubePulseCount, "0");
    store.update(Type::DeviceBatteryVoltage, "0.000");
    store.update(Type::DeviceBatteryPercent, "0");
    const auto body = scrape(store);
    for (const auto &name : numericNames)
        expect(body.find("\n" + name + " 0") != std::string::npos,
               "valid zero values must be exported");
}

void testEveryLabelEscapesSpecialCharacters()
{
    DeviceInfoStore store;
    store.setBridgeFirmware("bridge\\\"\n");
    store.update(Type::DeviceModel, "Vendor\\\"\n GC-01");
    store.update(Type::DeviceFirmware, "firmware\\\"\n");
    store.update(Type::DeviceId, "id\\\"\n");
    store.update(Type::DeviceLocale, "locale\\\"\n");
    store.update(Type::DevicePower, "power\\\"\n");
    const auto body = scrape(store);
    const char *labels[] = {
        "manufacturer=\"Vendor\\\\\\\"\\n\"",
        "model=\"Vendor\\\\\\\"\\n GC-01\"",
        "firmware=\"firmware\\\\\\\"\\n\"",
        "bridge_firmware=\"bridge\\\\\\\"\\n\"",
        "device_id=\"id\\\\\\\"\\n\"",
        "locale=\"locale\\\\\\\"\\n\"",
        "device_power=\"power\\\\\\\"\\n\""};
    for (const auto *label : labels)
        expect(body.find(label) != std::string::npos,
               "every label must escape backslashes, quotes and line feeds");
}

void testUtf8LabelsArePreserved()
{
    DeviceInfoStore store;
    store.update(Type::DeviceModel, "Bösean µSv");
    const auto body = scrape(store);
    expect(body.find("model=\"Bösean µSv\"") != std::string::npos,
           "UTF-8 label values must remain intact");
}

void dumpFixtures(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    const std::vector<std::string> cases = {
        "complete", "boot", "disconnected", "cleared", "missing_dose", "zero",
        "quotes", "backslashes", "newlines", "utf8"};
    for (const auto &name : cases)
    {
        DeviceInfoStore store;
        if (name != "boot")
            populate(store);
        if (name == "disconnected") store.clearLiveData();
        if (name == "cleared") store.clearMeasurements();
        if (name == "missing_dose") store.update(Type::TubeDoseRate, "");
        if (name == "zero") store.update(Type::TubeRate, "0");
        if (name == "quotes") store.update(Type::DeviceModel, "Vendor GC\"01");
        if (name == "backslashes") store.update(Type::DeviceModel, "Vendor GC\\01");
        if (name == "newlines") store.update(Type::DeviceModel, "Vendor GC\n01");
        if (name == "utf8") store.update(Type::DeviceModel, "Bösean µSv");
        std::ofstream(directory / (name + ".prom")) << scrape(store);
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--dump")
    {
        dumpFixtures(argv[2]);
        return 0;
    }
    const std::vector<std::pair<const char *, std::function<void()>>> tests = {
        {"boot", testBootOmitsUnavailableReadings},
        {"disconnect", testDisconnectOmitsAllLiveReadings},
        {"cleared measurements", testClearedMeasurementsPreserveBatteryReadings},
        {"independent missing readings", testEachMissingReadingIsOmittedIndependently},
        {"complete readings", testCompleteReadingsRetainPrecision},
        {"zero readings", testZeroReadingsRemainAvailable},
        {"label escaping", testEveryLabelEscapesSpecialCharacters},
        {"UTF-8 labels", testUtf8LabelsArePreserved}};
    int failures = 0;
    for (const auto &test : tests)
    {
        try { test.second(); }
        catch (const std::exception &error)
        {
            std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
            ++failures;
        }
    }
    if (!failures)
        std::cout << "device info Prometheus tests passed (8 cases)\n";
    return failures ? 1 : 0;
}
