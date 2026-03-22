#pragma once
#include <Arduino.h>

// USB-C PD target configuration for STUSB4500
// NOTE: The SBB A802 slave clock is spec'd for 24 V; USB-PD via STUSB4500
// is limited to 20 V.  20 V works in practice with SBB impulse clocks.
#define USB_PD2_VOLTAGE  20.0f  // PDO 2 voltage (V)
#define USB_PD2_CURRENT   2.0f  // PDO 2 current (A)
#define USB_PD3_VOLTAGE  20.0f  // PDO 3 voltage (V) — highest priority
#define USB_PD3_CURRENT   3.0f  // PDO 3 current (A)
#define USB_PD_MIN_READY 18.0f  // minimum voltage (V) considered "ready"

// Initialise the STUSB4500 and negotiate the target PD contract.
void USB_config();

// Returns true once the STUSB4500 initialised successfully and the
// configured PDO voltage is at or above USB_PD_MIN_READY.
bool USB_isVoltageReady();
