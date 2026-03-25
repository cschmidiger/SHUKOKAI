#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "network_manager.h"

static const unsigned long NTP_SYNC_INTERVAL = 10UL * 60 * 1000; // 10 min

static unsigned long lastNTPSync = 0;
static unsigned long lastMillis  = 0;

NetworkManager networkManager;

NetworkManager::NetworkManager()
{
    currentTimezoneIdentifier = defaultTimezoneIdentifier;
    currentPosixString        = defaultTimeZone;
}

void NetworkManager::begin()
{
    loadTimezone();

    if (!connectWiFi())
    {
        Serial.println("[Net] No WiFi - running on internal clock");
        currentHour = 12; currentMinute = 0; currentSecond = 0;
        lastMillis = millis();
        return;
    }

    Serial.println("[Net] WiFi connected: " + WiFi.localIP().toString());

    configTzTime(currentPosixString.c_str(), ntpServer);

    struct tm t;
    if (getLocalTime(&t))
    {
        currentHour   = t.tm_hour;
        currentMinute = t.tm_min;
        currentSecond = t.tm_sec;
        Serial.printf("[NTP] Time: %02d:%02d:%02d (%s)\n",
                      currentHour, currentMinute, currentSecond,
                      currentTimezoneIdentifier.c_str());
    }
    else
    {
        Serial.println("[NTP] Failed - using 12:00:00");
        currentHour = 12; currentMinute = 0; currentSecond = 0;
    }

    lastMillis  = millis();
    lastNTPSync = millis();
}

void NetworkManager::loop()
{
    unsigned long now = millis();

    if (now - lastMillis >= 1000)
    {
        lastMillis = now;
        incrementTime();
        if (currentSecond == 0)
            Serial.printf("[Net] %02d:%02d\n", currentHour, currentMinute);
    }

    if (now - lastNTPSync >= NTP_SYNC_INTERVAL)
    {
        lastNTPSync = now;
        if (WiFi.isConnected())
            syncNTP();
        else
            Serial.println("[NTP] No WiFi - skipping sync");
    }
}

bool NetworkManager::connectWiFi()
{
    preferences.begin("wifi", true);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("pass", "");
    preferences.end();

    if (ssid.length() == 0)
    {
        Serial.println("[Net] No credentials stored - starting config portal");
        startConfigPortal(); // never returns, restarts after save
        return false;
    }

    Serial.print("[Net] Connecting to: " + ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[Net] Connection failed - starting config portal");
        startConfigPortal(); // never returns, restarts after save
        return false;
    }

    return true;
}

void NetworkManager::startConfigPortal()
{
    Serial.println("[Net] Starting AP...");
    WiFi.mode(WIFI_AP);
    delay(100);

    bool apOk = WiFi.softAP("Nebenuhr");
    Serial.println("[Net] softAP result: " + String(apOk));
    Serial.println("[Net] AP IP: " + WiFi.softAPIP().toString());

    Serial.println("[Net] Creating WebServer...");
    WebServer server(80);
    Serial.println("[Net] Registering routes...");

    const char *html =
        "<!DOCTYPE html><html><head><title>Nebenuhr Setup</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
        "<body style='font-family:sans-serif;max-width:320px;margin:40px auto'>"
        "<h2>Nebenuhr WiFi Setup</h2>"
        "<form method='post' action='/save'>"
        "SSID:<br><input name='ssid' type='text' style='width:100%'><br><br>"
        "Password:<br><input name='pass' type='password' style='width:100%'><br><br>"
        "<input type='submit' value='Save and Connect' style='width:100%'>"
        "</form></body></html>";

    server.on("/", HTTP_GET, [&]() {
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, [&]() {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        if (ssid.length() == 0) {
            server.send(400, "text/html", "<h2>SSID cannot be empty</h2>");
            return;
        }
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("pass", pass);
        preferences.end();
        server.send(200, "text/html",
            "<h2>Saved! Connecting...</h2><p>The device will restart now.</p>");
        delay(1000);
        ESP.restart();
    });

    Serial.println("[Net] Starting server...");
    server.begin();
    Serial.println("[Net] Portal ready - connect to 'Nebenuhr', open http://192.168.4.1");

    while (true)
    {
        server.handleClient();
        delay(1);
    }
}

void NetworkManager::syncNTP()
{
    configTzTime(currentPosixString.c_str(), ntpServer);
    struct tm t;
    if (getLocalTime(&t))
    {
        currentHour   = t.tm_hour;
        currentMinute = t.tm_min;
        currentSecond = t.tm_sec;
        Serial.printf("[NTP] Synced: %02d:%02d:%02d (%s)\n",
                      currentHour, currentMinute, currentSecond,
                      currentTimezoneIdentifier.c_str());
    }
    else
    {
        Serial.println("[NTP] Sync failed - keeping last time");
    }
}

void NetworkManager::loadTimezone()
{
    preferences.begin("timezone", true);
    String id    = preferences.getString("identifier", "");
    String posix = preferences.getString("posix", "");
    preferences.end();

    if (id.length() > 0 && posix.length() > 0)
    {
        currentTimezoneIdentifier = id;
        currentPosixString        = posix;
        Serial.println("[Net] Timezone: " + id);
    }
}

void NetworkManager::incrementTime()
{
    if (++currentSecond >= 60) {
        currentSecond = 0;
        if (++currentMinute >= 60) {
            currentMinute = 0;
            if (++currentHour >= 24)
                currentHour = 0;
        }
    }
}
