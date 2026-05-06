// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "GmcMap/GmcMapLogRedaction.h"

namespace
{
void testRedactsAidAndGidInsideQueryString()
{
    const String query = "/log2.asp?AID=09049&GID=94667115109&CPM=28.617&ACPM=27.236&uSV=0.26413";
    const std::string redacted = GmcMapLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted.find("AID=***REDACTED***") != std::string::npos);
    assert(redacted.find("GID=***REDACTED***") != std::string::npos);
    assert(redacted.find("09049") == std::string::npos);
    assert(redacted.find("94667115109") == std::string::npos);
    assert(redacted.find("CPM=28.617") != std::string::npos);
    assert(redacted.find("uSV=0.26413") != std::string::npos);
}

void testLeavesQueriesWithoutCredentialsUntouched()
{
    const String query = "/log2.asp?CPM=28.617&ACPM=27.236&uSV=0.26413";
    const std::string redacted = GmcMapLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted == query.c_str());
}

void testRedactsAidAndGidAtEndOfQueryString()
{
    const String query = "/log2.asp?AID=09049&GID=94667115109";
    const std::string redacted = GmcMapLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted == "/log2.asp?AID=***REDACTED***&GID=***REDACTED***");
}
} // namespace

int main()
{
    testRedactsAidAndGidInsideQueryString();
    testLeavesQueriesWithoutCredentialsUntouched();
    testRedactsAidAndGidAtEndOfQueryString();
    std::cout << "gmcmap log redaction tests passed\n";
    return 0;
}
