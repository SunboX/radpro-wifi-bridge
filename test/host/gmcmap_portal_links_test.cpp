#include <cassert>
#include <iostream>
#include <string>

#include "GmcMap/GmcMapPortalLinks.h"

using GmcMapPortalLinks::buildGmcMapDeviceHistoryUrl;

namespace
{
void testReturnsEmptyUrlWhenDeviceIdMissing()
{
    assert(std::string(buildGmcMapDeviceHistoryUrl("")).empty());
}

void testBuildsDeviceHistoryUrlFromTrimmedDeviceId()
{
    const std::string url = buildGmcMapDeviceHistoryUrl("  94667115109  ").c_str();
    assert(url == "https://www.gmcmap.com/historyData.asp?Param_ID=94667115109&systemTimeZone=1");
}
} // namespace

int main()
{
    testReturnsEmptyUrlWhenDeviceIdMissing();
    testBuildsDeviceHistoryUrlFromTrimmedDeviceId();
    std::cout << "gmcmap portal link tests passed\n";
    return 0;
}
