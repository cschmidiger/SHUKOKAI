#include "buzzer.h"

Buzzer buzzer;

static void beep(int duration)
{
    ledcWriteTone(BUZZER_LEDC_CHANNEL, BUZZER_FREQUENCY);
    delay(duration);
    ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
}

Buzzer::Buzzer()
{
    buzzerState = true;
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_FREQUENCY, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
}

void Buzzer::alarm()
{
    for (int i = 0; i < 4; i++) {
        beep(BUZZER_DURATION);
        delay(100);
    }
    beep(buzzerState ? BUZZER_DURATION : 1000);
}

void Buzzer::click(int duration, int count)
{
    if (!buzzerState) return;
    for (int i = 0; i < count; i++) {
        beep(duration);
        delay(100);
    }
}
