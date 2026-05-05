// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationPortalView.h"

using OpenRadiationPortalView::LatestMeasurementViewModel;
using OpenRadiationPortalView::buildLatestMeasurementPage;
using OpenRadiationPortalView::buildLinksSection;
using OpenRadiationPortalView::buildMeasurementEnvironmentOptions;

namespace
{
void testLinksSectionIncludesMapAndLatestAnchors()
{
    const std::string html = buildLinksSection(
        "https://request.openradiation.net/openradiation/14/52.520008/13.404954",
        "/openradiation/latest");
    assert(html.find("Open on OpenRadiation map") != std::string::npos);
    assert(html.find("Open latest published measurement") != std::string::npos);
}

void testLinksSectionExplainsMissingLatestMeasurement()
{
    const std::string html = buildLinksSection(
        "https://request.openradiation.net/openradiation/14/52.520008/13.404954",
        "");
    assert(html.find("Latest published measurement becomes available after the first successful upload.") != std::string::npos);
}

void testLatestMeasurementErrorPageShowsBackLink()
{
    LatestMeasurementViewModel model;
    model.errorMessage = "No successful OpenRadiation publish has been recorded yet.";

    const std::string html = buildLatestMeasurementPage(model);
    assert(html.find("No successful OpenRadiation publish has been recorded yet.") != std::string::npos);
    assert(html.find("Back to OpenRadiation settings") != std::string::npos);
}

void testLatestMeasurementSuccessPageShowsMeasurementFields()
{
    LatestMeasurementViewModel model;
    model.reportUuid = "bbf2ff7c-83f8-4d62-a5ab-900e216bb170";
    model.valueText = "0.1234 uSv/h";
    model.startTime = "2026-04-17T18:30:00Z";
    model.qualification = "validated";
    model.mapUrl = "https://request.openradiation.net/openradiation/14/52.520008/13.404954";

    const std::string html = buildLatestMeasurementPage(model);
    assert(html.find("Latest OpenRadiation measurement") != std::string::npos);
    assert(html.find("bbf2ff7c-83f8-4d62-a5ab-900e216bb170") != std::string::npos);
    assert(html.find("0.1234 uSv/h") != std::string::npos);
    assert(html.find("validated") != std::string::npos);
    assert(html.find("Open on OpenRadiation map") != std::string::npos);
}

void testMeasurementEnvironmentOptionsMarkSelectedValue()
{
    const std::string html = buildMeasurementEnvironmentOptions("city");
    assert(html.find("<option value='city' selected>City</option>") != std::string::npos);
    assert(html.find("<option value='countryside'>Countryside</option>") != std::string::npos);
    assert(html.find("<option value='inside'>Inside</option>") != std::string::npos);
}
} // namespace

int main()
{
    testLinksSectionIncludesMapAndLatestAnchors();
    testLinksSectionExplainsMissingLatestMeasurement();
    testLatestMeasurementErrorPageShowsBackLink();
    testLatestMeasurementSuccessPageShowsMeasurementFields();
    testMeasurementEnvironmentOptionsMarkSelectedValue();
    std::cout << "openradiation portal view tests passed\n";
    return 0;
}
