#include <Arduino.h>
#include <SparkFun_STUSB4500.h>
#include "usb.h"

static STUSB4500 usb;
static bool _usbReady = false;

bool USB_isVoltageReady()
{
    return _usbReady;
}

void USB_config()
{
    const int maxRetries = 3;
    bool usbInitialized = false;

    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        Serial.println("[USB] Initialization attempt " + String(attempt) + " of " + String(maxRetries));

        if (usb.begin())
        {
            usbInitialized = true;
            break;
        }

        if (attempt < maxRetries)
        {
            Serial.println("[USB] STUSB4500 not found, retrying in 1 s...");
            delay(1000);
        }
    }

    if (!usbInitialized)
    {
        Serial.println("[USB] STUSB4500 not found after " + String(maxRetries) + " attempts. Continuing without USB-C PD.");
        return;
    }

    delay(100);
    usb.read();

    Serial.println();
    Serial.println("[USB] +-------------------------------------+");
    Serial.println("[USB] |       STUSB4500  connected          |");
    Serial.printf( "[USB] |  PDO2  %5.1f V  /  %.1f A              |\n", usb.getVoltage(2), usb.getCurrent(2));
    Serial.printf( "[USB] |  PDO3  %5.1f V  /  %.1f A              |\n", usb.getVoltage(3), usb.getCurrent(3));
    Serial.println("[USB] +-------------------------------------+");
    Serial.println();

    // Only write NVM when the stored values differ from the targets.
    // The STUSB4500 NVM has a limited write-cycle lifetime.
    //
    // FORCE NVM RECOVERY: comment the normal block below and uncomment
    // the force block, flash once, then restore and reflash.
    //
    // --- FORCE RECOVERY ---
    // bool needsWrite = true;
    // Serial.println("### STUSB4500 FORCE NVM RECOVERY — re-comment after this flash! ###");
    // --- end force recovery ---

    // --- NORMAL OPERATION ---
    bool needsWrite =
        (abs(usb.getVoltage(2) - USB_PD2_VOLTAGE) > 0.05f) ||
        (abs(usb.getCurrent(2) - USB_PD2_CURRENT) > 0.05f) ||
        (abs(usb.getVoltage(3) - USB_PD3_VOLTAGE) > 0.05f) ||
        (abs(usb.getCurrent(3) - USB_PD3_CURRENT) > 0.05f);
    // --- end normal operation ---

    if (!needsWrite)
    {
        Serial.println("[USB] NVM already correct, skipping write.");
    }
    else
    {
        Serial.println("[USB] NVM settings differ — updating.");

        usb.setPdoNumber(2);
        usb.setVoltage(2, USB_PD2_VOLTAGE);
        usb.setCurrent(2, USB_PD2_CURRENT);
        usb.setLowerVoltageLimit(2, 30);
        usb.setUpperVoltageLimit(2, 30);

        usb.setPdoNumber(3);
        usb.setVoltage(3, USB_PD3_VOLTAGE);
        usb.setCurrent(3, USB_PD3_CURRENT);
        usb.setLowerVoltageLimit(3, 30);
        usb.setUpperVoltageLimit(3, 30);

        usb.write();
        // softReset() intentionally omitted — it causes a momentary power
        // dropout that can brownout the ESP32.
    }

    int activePdo = usb.getPdoNumber();
    float activeVoltage = usb.getVoltage(activePdo);
    Serial.println("[USB] Active PDO: " + String(activePdo) + "  Voltage: " + String(activeVoltage) + " V");

    if (activeVoltage >= USB_PD_MIN_READY)
    {
        _usbReady = true;
        Serial.println("[USB] Voltage ready (" + String(activeVoltage) + " V >= " + String(USB_PD_MIN_READY) + " V).");
    }
    else
    {
        Serial.println("[USB] Warning: negotiated voltage " + String(activeVoltage) +
                       " V is below minimum " + String(USB_PD_MIN_READY) +
                       " V. Check your power adapter.");
    }
}
