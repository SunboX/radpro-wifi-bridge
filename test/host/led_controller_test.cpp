#include <cassert>
#include <iostream>

#include "Arduino.h"
#include "Led/LedController.h"

static void testErrorModeSuppressesGreenPulse()
{
    LedController led;
    led.begin();

    setMillis(1000);
    led.setMode(LedMode::Error);
    led.triggerPulse(LedPulse::MqttSuccess, 150);
    led.update();

    assert(g_neopixel_r > 0);
    assert(g_neopixel_g > 0);
    assert(g_neopixel_b == 0);
    assert(g_neopixel_r >= g_neopixel_g);
}

static void testHealthyModeKeepsGreenPulse()
{
    LedController led;
    led.begin();

    setMillis(2000);
    led.setMode(LedMode::DeviceReady);
    led.triggerPulse(LedPulse::MqttSuccess, 150);
    led.update();

    assert(g_neopixel_r == 0);
    assert(g_neopixel_g > 0);
    assert(g_neopixel_b == 0);
}

int main()
{
    testErrorModeSuppressesGreenPulse();
    testHealthyModeKeepsGreenPulse();
    std::cout << "LED controller tests passed\n";
    return 0;
}
