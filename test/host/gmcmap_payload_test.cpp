#include <cassert>
#include <iostream>
#include <string>

#include "GmcMap/GmcMapPayload.h"

namespace
{
void testBuildsLogQueryWithIntegerCpmValues()
{
    const String query = GmcMapPayload::buildLogQuery(
        "09049",
        "94667115109",
        28.617f,
        27.236f,
        "0.26413");

    assert(std::string(query.c_str()) == "/log2.asp?AID=09049&GID=94667115109&CPM=29&ACPM=27&uSV=0.26413");
}

void testRoundsCpmToNearestInteger()
{
    assert(std::string(GmcMapPayload::formatCpm(28.49f).c_str()) == "28");
    assert(std::string(GmcMapPayload::formatCpm(28.50f).c_str()) == "29");
    assert(std::string(GmcMapPayload::formatCpm(0.0f).c_str()) == "0");
}
} // namespace

int main()
{
    testBuildsLogQueryWithIntegerCpmValues();
    testRoundsCpmToNearestInteger();
    std::cout << "gmcmap payload tests passed\n";
    return 0;
}
