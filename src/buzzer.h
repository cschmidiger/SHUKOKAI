#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

#define BUZZER_PIN          15
#define BUZZER_DURATION     20    // ms
#define BUZZER_FREQUENCY    5000  // Hz
#define BUZZER_LEDC_CHANNEL 0     // LEDC channel (0–15 available on ESP32)

class Buzzer
{
public:
    Buzzer();
    bool buzzerState;
    void click(int duration, int count);
    void alarm();
};

extern Buzzer buzzer;

#endif // BUZZER_H
