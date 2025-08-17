#include <Arduino.h>

#define RGB_BUILTIN 48 // Onboard WS2812 LED pin

// --- Configuration ---
#define INITIAL_STARTUP_DELAY_MS 5000
#define ALLOW_EARLY_START true

// --- Program State ---
bool isRunning = false;
unsigned long startupDelayMs = INITIAL_STARTUP_DELAY_MS;
unsigned long startupStartTime = 0;

// Forward declarations for our new functions
void handleStartupLogic();
void runMainLogic();

void setup()
{
    // Serial -> second USB Port
    Serial0.begin(115200); // Debug Serial port

    // Record the start time for our non-blocking delay.
    startupStartTime = millis();

    Serial0.println("Initializing RadPro WiFi Bridge...");
    if (ALLOW_EARLY_START)
    {
        Serial0.println("Send 'start' or a 'delay <ms>' command to begin.");
    }
}

void loop()
{
    // This state machine makes the program's flow easy to understand.
    if (!isRunning)
    {
        handleStartupLogic();
    }
    else
    {
        runMainLogic();
    }
}

/**
 * @brief Handles all logic that should run *before* the main application starts.
 * This includes checking for the timeout, handling user commands for early start,
 * or reconfiguring the delay.
 */
void handleStartupLogic()
{
    // Condition 1: Startup timer has elapsed.
    if (millis() - startupStartTime >= startupDelayMs)
    {
        isRunning = true;
    }

    // Condition 2: The user sends a command over Serial0.
    if (Serial0.available() > 0)
    {
        String command = Serial0.readStringUntil('\n');
        command.trim(); // Clean up any whitespace.

        // The user wants to change the startup delay.
        if (command.startsWith("delay "))
        {
            long newDelay = command.substring(6).toInt();
            if (newDelay > 0)
            {
                startupDelayMs = newDelay;
                startupStartTime = millis(); // Reset timer to apply the new delay.
                Serial0.print("Startup delay updated to: ");
                Serial0.print(startupDelayMs);
                Serial0.println(" ms");
            }
        }
        // Any other input will trigger an early start.
        else if (ALLOW_EARLY_START && command.length() > 0)
        {
            isRunning = true;
            Serial0.println("Early start triggered by user!");
        }
    }

    // If startup is now complete (either by timer or user), print the final message.
    if (isRunning)
    {
        Serial0.println("Starting RadPro WiFi Bridge...");
        return; // Exit this function.
    }

    // --- Periodic Countdown Message ---
    // This runs only if we are still waiting for startup.
    static unsigned long lastCountdownTime = 0;
    if (millis() - lastCountdownTime >= 1000)
    {
        lastCountdownTime = millis();
        unsigned long remaining = (startupDelayMs - (millis() - startupStartTime)) / 1000;
        Serial0.print("Starting in ");
        Serial0.print(remaining);
        Serial0.println(" seconds...");
    }
}

/**
 * @brief Handles the main application logic that runs repeatedly *after* startup.
 */
void runMainLogic()
{
    // --- Task 1: Flash RGB LED every 250ms ---
    static unsigned long lastLedTime = 0;
    if (millis() - lastLedTime >= 250)
    {
        lastLedTime = millis();
        uint8_t r = 0;
        uint8_t g = 10;
        uint8_t b = 0;
        neopixelWrite(RGB_BUILTIN, r, g, b); // Set LED to a random color
    }

    // --- Task 2: Placeholder for other recurring tasks (runs every 1000ms) ---
    static unsigned long lastLoopTime = 0;
    if (millis() - lastLoopTime >= 1000)
    {
        lastLoopTime = millis();
        Serial0.println("Main loop is running.");
        // Add other non-blocking tasks here.
    }
}