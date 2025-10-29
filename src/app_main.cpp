#include <Arduino.h>

// Forward declarations (Arduino.h usually declares these, but this is explicit)
void setup();
void loop();

extern "C" void app_main(void)
{
    // Initialize Arduino (pins, serial, timers, etc.)
    initArduino();

    // Run your Arduino sketch entry points
    setup();
    for (;;)
    {
        loop();
        // Yield so other FreeRTOS tasks (IDF + yours) can run
        delay(1); // or: vTaskDelay(1);
    }
}