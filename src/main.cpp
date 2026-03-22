#include <Arduino.h>
#include <Wire.h>
#include "soc/rtc_cntl_reg.h"  // for brownout disable
#include "usb.h"
#include "buzzer.h"
#include "network_manager.h"

// ── Slave-clock output — TB6612 H-bridge ─────────────────────────────────────
// TB6612 drives the SBB A802 slave clock coil with alternating-polarity 20 V
// impulses.  Polarity flips every minute as the A802 spec requires.
//   AIN1  AIN2  PWM   → motor action
//   HIGH  LOW   HIGH  → +20 V (forward)
//   LOW   HIGH  HIGH  → −20 V (reverse)
//   x     x     LOW   → coast (off)
#define MOT_PWM          5
#define MOT_AIN1        14
#define MOT_AIN2        19
#define MOT_STBY        26
#define SLAVE_CLOCK_PULSE_MS 200  // 100 ms — standard SBB impulse width

// Set to 60 for production (one pulse per minute = one step per minute).
// Reduce for testing, e.g. 5 = pulse every 5 seconds.
static const int PULSE_INTERVAL_S = 10;

static unsigned long lastFireMs   = 0;
static bool polarityState = false;  // false = +pulse, true = −pulse

static void fireSlaveClock()
{
    // Set H-bridge direction before enabling PWM
    if (polarityState)
    {
        digitalWrite(MOT_AIN1, LOW);
        digitalWrite(MOT_AIN2, HIGH);
    }
    else
    {
        digitalWrite(MOT_AIN1, HIGH);
        digitalWrite(MOT_AIN2, LOW);
    }

    digitalWrite(MOT_PWM, HIGH);
    delay(SLAVE_CLOCK_PULSE_MS);
    digitalWrite(MOT_PWM, LOW);   // coast — coil de-energised
    digitalWrite(MOT_AIN1, LOW);
    digitalWrite(MOT_AIN2, LOW);

    buzzer.click(BUZZER_DURATION, 1);  // single tick = clock pulse sent

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
    // Disable brownout detector — WiFi radio draws ~300 mA peaks which dips
    // the 3.3V rail below the default threshold. Hardware fix: add 100-470 uF
    // bulk cap on 3.3V rail close to the ESP32.
    REG_WRITE(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(2000);  // pause so serial monitor can connect before output begins
    Serial.println("\n[Boot] Nebenuhr starting...");

    pinMode(MOT_PWM,  OUTPUT);
    pinMode(MOT_AIN1, OUTPUT);
    pinMode(MOT_AIN2, OUTPUT);
    pinMode(MOT_STBY, OUTPUT);
    digitalWrite(MOT_PWM,  LOW);
    digitalWrite(MOT_AIN1, LOW);
    digitalWrite(MOT_AIN2, LOW);
    digitalWrite(MOT_STBY, HIGH);  // enable TB6612

    delay(5000);  // wait for power to stabilize before checking USB voltage
    buzzer.click(BUZZER_DURATION, 2);  // two short beeps = boot

    // ── I2C scan (diagnostic — remove once STUSB4500 is confirmed working) ──
    Wire.begin(21, 22);  // SDA, SCL — adjust if your PCB uses different pins
    Serial.println("[I2C] Scanning bus...");
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.printf("[I2C] Device at 0x%02X%s\n", addr,
                          addr == 0x28 ? "  <-- STUSB4500" : "");
            found = true;
        }
    }
    if (!found) Serial.println("[I2C] No devices found");

    USB_config();

    if (USB_isVoltageReady())
        buzzer.click(BUZZER_DURATION, 1);   // single beep = USB voltage OK
    else
        buzzer.alarm();        // alarm pattern = USB voltage not ready

    networkManager.begin();
    buzzer.click(BUZZER_DURATION, 3);  // three beeps = WiFi + NTP done
}

void loop()
{
    networkManager.loop();

    unsigned long now = millis();
    if (now - lastFireMs >= (unsigned long)PULSE_INTERVAL_S * 1000UL)
    {
        lastFireMs = now;
        if (USB_isVoltageReady())
        {
            fireSlaveClock();
        }
        else
        {
            Serial.printf("[SlaveClk] %02d:%02d — skipped (USB voltage not ready)\n",
                          networkManager.currentHour,
                          networkManager.currentMinute);
        }
    }
}
