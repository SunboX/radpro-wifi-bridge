// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "OpenSenseMap/OpenSenseMapPortalLinks.h"

using OpenSenseMapPortalLinks::buildOpenSenseMapBoxUrl;
using OpenSenseMapPortalLinks::buildOpenSenseMapSensorSettingsUrl;

static void testReturnsEmptyBoxUrlWhenBoxIdMissing()
{
    assert(std::string(buildOpenSenseMapBoxUrl("")).empty());
}

static void testBuildsExploreUrlFromTrimmedBoxId()
{
    const std::string url = buildOpenSenseMapBoxUrl("  box-123  ");
    assert(url == "https://opensensemap.org/explore/box-123");
}

static void testReturnsEmptyUrlWhenBoxIdMissing()
{
    assert(std::string(buildOpenSenseMapSensorSettingsUrl("", "sensor-123")).empty());
}

static void testReturnsEmptyUrlWhenSensorIdMissing()
{
    assert(std::string(buildOpenSenseMapSensorSettingsUrl("box-123", "")).empty());
}

static void testBuildsSettingsUrlFromTrimmedIds()
{
    const std::string url = buildOpenSenseMapSensorSettingsUrl("  box-123  ", "  sensor-123  ");
    assert(url == "https://opensensemap.org/account/box-123/edit/sensors");
}

int main()
{
    testReturnsEmptyBoxUrlWhenBoxIdMissing();
    testBuildsExploreUrlFromTrimmedBoxId();
    testReturnsEmptyUrlWhenBoxIdMissing();
    testReturnsEmptyUrlWhenSensorIdMissing();
    testBuildsSettingsUrlFromTrimmedIds();
    std::cout << "openSenseMap portal link tests passed\n";
    return 0;
}
