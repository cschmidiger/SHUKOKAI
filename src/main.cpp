#include <Arduino.h>
#include <Wire.h>
#include "soc/rtc_cntl_reg.h"  // for brownout disable
#include "usb.h"
#include "buzzer.h"
#include "network_manager.h"

// ── Slave-clock output ────────────────────────────────────────────────────────
// GPIO driving the MOSFET / relay that switches 20 V to the SBB A802.
// The A802 is spec'd for 24 V alternating-polarity impulses; 20 V (USB-PD max)
// works in practice.  True alternating polarity requires an H-bridge with a
// second GPIO — see polarityState below for the tracking variable.
#define SLAVE_CLOCK_PIN      12
#define SLAVE_CLOCK_PULSE_MS 500  // 100 ms — standard SBB impulse width

static int  lastSecond    = -1;
static bool polarityState = false;  // flips every minute; use for H-bridge direction

static void fireSlaveClock()
{
    // TODO: for full A802 compliance add a second GPIO + H-bridge so the
    // polarity alternates every minute (polarityState already tracks it).
    digitalWrite(SLAVE_CLOCK_PIN, HIGH);
    delay(SLAVE_CLOCK_PULSE_MS);
    digitalWrite(SLAVE_CLOCK_PIN, LOW);

    polarityState = !polarityState;
    buzzer.click(BUZZER_DURATION, 1);  // single tick = clock pulse sent

    Serial.printf("[SlaveClk] %02d:%02d — pulse fired (%d ms, polarity: %s)\n",
                  networkManager.currentHour,
                  networkManager.currentMinute,
                  SLAVE_CLOCK_PULSE_MS,
                  polarityState ? "+" : "-");
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

    pinMode(SLAVE_CLOCK_PIN, OUTPUT);
    digitalWrite(SLAVE_CLOCK_PIN, LOW);

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

    // Detect the leading edge of second == 0 (minute rollover).
    // networkManager keeps currentSecond in sync with NTP.
    int sec = networkManager.currentSecond;

    if (sec == 0 && lastSecond != 0)
    {
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

    lastSecond = sec;
}
