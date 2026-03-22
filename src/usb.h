#pragma once
#include <Arduino.h>

// USB-C PD target configuration for STUSB4500
// NOTE: TB6612 VM absolute maximum is 15 V — do not exceed 13.5 V operating.
// 12 V is well within spec and sufficient for the SBB A802 slave clock coil.
#define USB_PD2_VOLTAGE  12.0f  // PDO 2 voltage (V)
#define USB_PD2_CURRENT   2.0f  // PDO 2 current (A)
#define USB_PD3_VOLTAGE  12.0f  // PDO 3 voltage (V) — highest priority
#define USB_PD3_CURRENT   3.0f  // PDO 3 current (A)
#define USB_PD_MIN_READY 10.0f  // minimum voltage (V) considered "ready"

// Initialise the STUSB4500 and negotiate the target PD contract.
void USB_config();

// Returns true once the STUSB4500 initialised successfully and the
// configured PDO voltage is at or above USB_PD_MIN_READY.
bool USB_isVoltageReady();
