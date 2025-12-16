/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_BUZZER_H
#define YARR_BUZZER_H

#include "YarrboardConfig.h"
#include "driver/ledc.h"
#include <ArduinoJson.h>

struct Note {
    uint16_t freqHz;
    uint16_t ms;
}; // freq=0 => rest

struct Melody {
    const char* name;
    const Note* seq;
    size_t len;
};

#define YB_MAX_MELODY_LENGTH 100
#define LEDC_RES_BITS        10
#define BUZZER_DUTY          512 // ~50% at 10-bit
#define MELODY_ENTRY(x)      {#x, x, sizeof(x) / sizeof(Note)}

class YarrboardApp;
class ConfigManager;

void BuzzerTask(void* /*pv*/);

class BuzzerController
{
  public:
    BuzzerController(YarrboardApp& app, ConfigManager& config);

    void setup();
    void loop();

    bool playMelodyByName(const char* melody);
    void generateMelodyJSON(JsonVariant output);

  private:
    YarrboardApp& _app;
    ConfigManager& _config;

#ifdef YB_HAS_PIEZO
    // our global note buffer
    static Note g_noteBuffer[YB_MAX_MELODY_LENGTH]; // pick a safe max size
    static size_t g_noteCount = 0;

    // ---------- Buzzer task control ----------
    static TaskHandle_t buzzerTaskHandle = nullptr;
    static const Note* g_seq = nullptr;
    static size_t g_len = 0;
    static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

  #ifdef YB_PIEZO_ACTIVE
    bool piezoIsActive = true;
  #elif defined(YB_PIEZO_PASSIVE)
    bool piezoIsActive = false;
  #else
    bool piezoIsActive = false;
  #endif

#endif

    void playMelody(const Note* seq, size_t len);
    void buzzerMute();
    void buzzerTone(uint16_t freqHz);
};

#endif /* !YARR_BUZZER_H */