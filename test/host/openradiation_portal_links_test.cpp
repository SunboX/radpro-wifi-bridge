// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationPortalLinks.h"

using OpenRadiationPortalLinks::buildOpenRadiationMapUrl;

namespace
{
void testReturnsEmptyMapUrlWhenCoordinatesMissing()
{
    assert(std::string(buildOpenRadiationMapUrl(0.0f, 0.0f).c_str()).empty());
}

void testReturnsEmptyMapUrlWhenCoordinatesOutOfRange()
{
    assert(std::string(buildOpenRadiationMapUrl(91.0f, 13.0f).c_str()).empty());
    assert(std::string(buildOpenRadiationMapUrl(52.5f, 181.0f).c_str()).empty());
}

void testBuildsMapUrlFromCoordinates()
{
    const std::string url = buildOpenRadiationMapUrl(52.520008f, 13.404954f);
    assert(url == "https://request.openradiation.net/openradiation/14/52.520008/13.404954");
}
} // namespace

int main()
{
    testReturnsEmptyMapUrlWhenCoordinatesMissing();
    testReturnsEmptyMapUrlWhenCoordinatesOutOfRange();
    testBuildsMapUrlFromCoordinates();
    std::cout << "openradiation portal link tests passed\n";
    return 0;
}
