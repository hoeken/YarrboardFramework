/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_BUZZER_H
#define YARR_BUZZER_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include "driver/ledc.h"
#include <ArduinoJson.h>

struct Note {
    uint16_t freqHz;
    uint16_t ms;
};

struct Melody {
    const char* name;
    const Note* seq;
    size_t len;
};

#define YB_MAX_MELODY_LENGTH 100
#define LEDC_RES_BITS        10
#define BUZZER_DUTY          512
#define MELODY_ENTRY(x)      {#x, x, sizeof(x) / sizeof(Note)}

class YarrboardApp;
class ConfigManager;

// Forward declaration
void BuzzerTask(void* pv);

class BuzzerController : public BaseController
{
  public:
    BuzzerController(YarrboardApp& app);

    bool setup() override;

    bool playMelodyByName(const char* melody);
    void generateMelodyJSON(JsonVariant output);

    // Make the task a friend so it can access private static members
    friend void BuzzerTask(void* pv);

  private:
#ifdef YB_HAS_PIEZO
    // --- DECLARATIONS ONLY (No assignments here) ---

    // our global note buffer
    static Note g_noteBuffer[YB_MAX_MELODY_LENGTH];
    static size_t g_noteCount;

    // Buzzer task control
    static TaskHandle_t buzzerTaskHandle;
    static const Note* g_seq;
    static size_t g_len;
    static portMUX_TYPE g_mux;

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