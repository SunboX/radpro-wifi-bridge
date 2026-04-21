#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "Arduino.h"
#include "DeviceManager.h"

namespace
{
void expectLastCommand(const UsbCdcHost &host, const char *expected)
{
    const auto &commands = host.sentCommands();
    assert(!commands.empty());
    assert(commands.back() == expected);
}

void testStartupSkipsUnsupportedHvMetadataQueries()
{
    setMillis(0);

    UsbCdcHost host;
    DeviceManager manager(host);
    manager.begin(std::vector<std::pair<uint16_t, uint16_t>>{});
    manager.start();

    host.simulateConnect();

    advanceMillis(100);
    manager.loop();

    const auto &initialCommands = host.sentCommands();
    assert(initialCommands.size() == 3);
    assert(initialCommands[0] == "GET deviceId\r\n");
    assert(initialCommands[1] == "GET deviceId\n");
    assert(initialCommands[2] == "GET deviceId\r");

    host.simulateLine("OK FNIRSI GC-01;Rad Pro 3.1/en;gc01-123456");
    expectLastCommand(host, "GET devicePower\r\n");

    host.simulateLine("OK 1");
    expectLastCommand(host, "GET deviceBatteryVoltage\r\n");

    host.simulateLine("OK 4.100");
    expectLastCommand(host, "GET deviceTime\r\n");

    host.simulateLine("OK 1700000000");
    expectLastCommand(host, "GET deviceTimeZone\r\n");

    host.simulateLine("OK 1.0");
    expectLastCommand(host, "GET tubeTime\r\n");

    host.simulateLine("OK 16000");
    expectLastCommand(host, "GET tubeSensitivity\r\n");

    host.simulateLine("OK 153.800");
    expectLastCommand(host, "GET tubeDeadTime\r\n");

    host.simulateLine("OK 0.0002420");
    expectLastCommand(host, "GET tubeDeadTimeCompensation\r\n");

    host.simulateLine("OK 0.0002500");

    for (const std::string &command : host.sentCommands())
    {
        assert(command.find("tubeHVFrequency") == std::string::npos);
        assert(command.find("tubeHVDutyCycle") == std::string::npos);
    }
}
} // namespace

int main()
{
    testStartupSkipsUnsupportedHvMetadataQueries();
    std::cout << "device manager GC-01 metadata tests passed\n";
    return 0;
}
