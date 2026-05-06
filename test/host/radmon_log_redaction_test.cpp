// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "Radmon/RadmonLogRedaction.h"

namespace
{
void testRedactsPasswordValueInsideQueryString()
{
    const String query = "/radmon.php?function=submit&user=SunboX&password=qUJtkin5qBgvr4E&value=26.292&unit=CPM&value2=0.24267&unit2=uSv/h";
    const std::string redacted = RadmonLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted.find("password=***REDACTED***") != std::string::npos);
    assert(redacted.find("qUJtkin5qBgvr4E") == std::string::npos);
    assert(redacted.find("user=SunboX") != std::string::npos);
    assert(redacted.find("value=26.292") != std::string::npos);
}

void testLeavesQueriesWithoutPasswordUntouched()
{
    const String query = "/radmon.php?function=submit&user=SunboX&value=26.292&unit=CPM";
    const std::string redacted = RadmonLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted == query.c_str());
}

void testRedactsPasswordAtEndOfQueryString()
{
    const String query = "/radmon.php?function=submit&user=SunboX&password=qUJtkin5qBgvr4E";
    const std::string redacted = RadmonLogRedaction::redactQueryForLogs(query).c_str();

    assert(redacted == "/radmon.php?function=submit&user=SunboX&password=***REDACTED***");
}
} // namespace

int main()
{
    testRedactsPasswordValueInsideQueryString();
    testLeavesQueriesWithoutPasswordUntouched();
    testRedactsPasswordAtEndOfQueryString();
    std::cout << "radmon log redaction tests passed\n";
    return 0;
}
