#include <cassert>
#include <iostream>
#include <string>

#include "Radmon/RadmonPortalLinks.h"

using RadmonPortalLinks::buildRadmonStationUrl;

namespace
{
void testReturnsEmptyUrlWhenUserMissing()
{
    assert(std::string(buildRadmonStationUrl("")).empty());
}

void testBuildsStationUrlFromTrimmedUser()
{
    const std::string url = buildRadmonStationUrl("  SunboX  ").c_str();
    assert(url == "https://radmon.org/radmon.php?function=showuserpage&user=SunboX");
}
} // namespace

int main()
{
    testReturnsEmptyUrlWhenUserMissing();
    testBuildsStationUrlFromTrimmedUser();
    std::cout << "radmon portal link tests passed\n";
    return 0;
}
