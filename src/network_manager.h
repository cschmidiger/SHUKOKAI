#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

class NetworkManager
{
public:
    int currentHour   = 0;
    int currentMinute = 0;
    int currentSecond = 0;

    NetworkManager();
    void begin();
    void loop();

private:
    Preferences preferences;

    const char *ntpServer                 = "pool.ntp.org";
    const char *defaultTimeZone           = "CET-1CEST,M3.5.0/2,M10.5.0/3";
    const char *defaultTimezoneIdentifier = "Europe/Zurich";

    String currentTimezoneIdentifier;
    String currentPosixString;

    bool connectWiFi();
    void startConfigPortal();
    void syncNTP();
    void loadTimezone();
    void incrementTime();
};

extern NetworkManager networkManager;

#endif // NETWORK_MANAGER_H
