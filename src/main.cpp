#include <Arduino.h>
#include "soc/rtc_cntl_reg.h"  // for brownout disable
#include "network_manager.h"

#define LED_BUILTIN_PIN 2

static void ledBlink(int count)
{
    for (int i = 0; i < count; i++)
    {
        digitalWrite(LED_BUILTIN_PIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN_PIN, LOW);
        delay(150);
    }
}

// ── Slave-clock output — BTS7960 43A H-bridge ────────────────────────────────
// The BTS7960 drives the slave clock coil with alternating-polarity impulses.
// Polarity flips every minute as the SBB/Nebenuhr spec requires.
//
//   RPWM HIGH, LPWM LOW  → +24 V on M+ (forward)
//   RPWM LOW,  LPWM HIGH → −24 V on M+ (reverse)
//   RPWM LOW,  LPWM LOW  → coil off (coast)
//
// R_EN and L_EN must be HIGH to enable the bridge.
#define MOT_RPWM   25   // GPIO25 → BTS7960 RPWM
#define MOT_LPWM   26   // GPIO26 → BTS7960 LPWM
#define MOT_R_EN   27   // GPIO27 → BTS7960 R_EN
#define MOT_L_EN   14   // GPIO14 → BTS7960 L_EN

#define SLAVE_CLOCK_PULSE_MS 200  // 200 ms impulse width (standard Nebenuhr)

// Set to 60 for production (one pulse per minute = one step per minute).
// Reduce for testing, e.g. 5 = pulse every 5 seconds.
static const int PULSE_INTERVAL_S = 60;

static unsigned long lastFireMs = 0;
static bool polarityState = false;  // false = +pulse, true = −pulse

static void fireSlaveClock()
{
    // Apply direction
    if (polarityState)
    {
        digitalWrite(MOT_RPWM, LOW);
        digitalWrite(MOT_LPWM, HIGH);
    }
    else
    {
        digitalWrite(MOT_RPWM, HIGH);
        digitalWrite(MOT_LPWM, LOW);
    }

    delay(SLAVE_CLOCK_PULSE_MS);

    // De-energise coil
    digitalWrite(MOT_RPWM, LOW);
    digitalWrite(MOT_LPWM, LOW);

    ledBlink(1);  // 1 blink = clock pulse sent

    Serial.printf("[SlaveClk] %02d:%02d — pulse fired (%d ms, polarity: %s)\n",
                  networkManager.currentHour,
                  networkManager.currentMinute,
                  SLAVE_CLOCK_PULSE_MS,
                  polarityState ? "-" : "+");

    polarityState = !polarityState;
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup()
{
    // Disable brownout detector — WiFi radio draws ~300 mA peaks
    REG_WRITE(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(5000);  // give time to open serial monitor
    Serial.println("\n[Boot] Nebenuhr starting...");

    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW);

    // BTS7960 control pins
    pinMode(MOT_RPWM, OUTPUT);
    pinMode(MOT_LPWM, OUTPUT);
    pinMode(MOT_R_EN, OUTPUT);
    pinMode(MOT_L_EN, OUTPUT);

    digitalWrite(MOT_RPWM, LOW);
    digitalWrite(MOT_LPWM, LOW);
    digitalWrite(MOT_R_EN, HIGH);   // enable right half-bridge
    digitalWrite(MOT_L_EN, HIGH);   // enable left half-bridge

    ledBlink(2);  // 2 blinks = boot OK

    networkManager.begin();
    ledBlink(3);  // 3 blinks = WiFi + NTP done
}

void loop()
{
    networkManager.loop();

    unsigned long now = millis();
    if (now - lastFireMs >= (unsigned long)PULSE_INTERVAL_S * 1000UL)
    {
        lastFireMs = now;
        fireSlaveClock();
    }
}
