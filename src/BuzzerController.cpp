/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "BuzzerController.h"
#include "YarrboardDebug.h"

BuzzerController::BuzzerController(YarrboardApp& app, ConfigManager& config) : _app(app),
                                                                               _config(config)
{
}

void BuzzerController::setup()
{
#ifdef YB_HAS_PIEZO
  pinMode(YB_PIEZO_PIN, OUTPUT);

  if (!piezoIsActive) {
    // LEDC once
    if (!ledcAttach(YB_PIEZO_PIN, 1000, LEDC_RES_BITS)) {
      YBP.println("Error attaching piezo to LEDC channel.");
      return;
    } else {
      piezoIsActive = false;
    }
  }

  // shh.
  buzzerMute();

  // Create buzzer task (small stack, low priority)
  // You can pin it to a core if you like; here we let the scheduler decide.
  xTaskCreate(
    BuzzerTask,
    "buzzer_task",
    2048, // stack words
    nullptr,
    1, // low priority is fine
    &buzzerTaskHandle);

  playMelodyByName(startup_melody);
#endif
}

void BuzzerController::loop()
{
}

void BuzzerController::generateMelodyJSON(JsonVariant output)
{
#ifdef YB_HAS_PIEZO
  output["melodies"][0] = "NONE";
  for (size_t i = 0; i < melodyCount; i++)
    output["melodies"][i + 1] = melodyTable[i].name;
#endif
}

bool BuzzerController::playMelodyByName(const char* melody)
{
#ifdef YB_HAS_PIEZO
  if (!strcmp(melody, "NONE"))
    return true;

  const Note* seq = nullptr;
  size_t len = 0;

  for (size_t i = 0; i < melodyCount; i++) {
    if (!strcmp(melody, melodyTable[i].name)) {
      playMelody(melodyTable[i].seq, melodyTable[i].len);
      return true;
    }
  }
  return false;
#else
  return true;
#endif
}

// Signal the task to start (or restart) a melody.
// Safe to call from loop/ISRs (but here we use regular context).
void BuzzerController::playMelody(const Note* seq, size_t len)
{
#ifdef YB_HAS_PIEZO
  // wait until we're done playing.
  if (g_len > 0 || g_seq != nullptr)
    return;

  // max size
  g_len = len;
  if (g_len > YB_MAX_MELODY_LENGTH)
    return;

  portENTER_CRITICAL(&g_mux);
  // copy into persistent storage
  for (size_t i = 0; i < len; i++) {
    g_noteBuffer[i] = seq[i];
  }
  g_seq = g_noteBuffer;
  portEXIT_CRITICAL(&g_mux);

  if (buzzerTaskHandle)
    xTaskNotifyGive(buzzerTaskHandle); // wake the task
#endif
}

// ---------- Low-level helpers ----------
void BuzzerController::buzzerMute()
{
#ifdef YB_HAS_PIEZO
  if (piezoIsActive)
    digitalWrite(YB_PIEZO_PIN, LOW);
  else
    ledcWrite(YB_PIEZO_PIN, 0);
#endif
}

void BuzzerController::buzzerTone(uint16_t freqHz)
{
#ifdef YB_HAS_PIEZO
  if (freqHz == 0) {
    buzzerMute(); // rest
  } else {
    if (piezoIsActive) {
      digitalWrite(YB_PIEZO_PIN, HIGH);
    } else {
      ledcWriteTone(YB_PIEZO_PIN, freqHz); // hardware retune
    }
  }
#endif
}

// ---------- FreeRTOS task ----------
void BuzzerTask(void* /*pv*/)
{
#ifdef YB_HAS_PIEZO
  // Sleep until someone calls playMelody()
  for (;;) {
    // Block here until notified OR timeout (so we can gracefully mute if nothing to do)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Snapshot the current request
    portENTER_CRITICAL(&g_mux);
    const Note* seq = g_seq;
    size_t len = g_len;
    portEXIT_CRITICAL(&g_mux);

    if (!seq || len == 0) {
      // Nothing to play
      buzzerMute();
      continue;
    }

    for (size_t i = 0; i < len; ++i) {
      // Do the note
      buzzerTone(seq[i].freqHz);

      // Precise sleep for note duration (only this task sleeps)
      TickType_t xLastWake = xTaskGetTickCount();
      TickType_t durTicks = pdMS_TO_TICKS(seq[i].ms);
      if (durTicks > 0)
        vTaskDelayUntil(&xLastWake, durTicks);
    }

    buzzerMute();

    // we're done.
    portENTER_CRITICAL(&g_mux);
    g_seq = nullptr;
    g_len = 0;
    portEXIT_CRITICAL(&g_mux);
  }
#endif
}

#ifdef YB_HAS_PIEZO

// Example melody (C scale up)
static const Note STARTUP[] = {
  {262, 100},
  {294, 100},
  {330, 100},
  {349, 100},
  {392, 100},
  {440, 100},
  {494, 100},
  {523, 100},
};

// Nice for power-on tones.
static const Note STARTUP_2[] = {
  {330, 120}, // E4
  {392, 120}, // G4
  {523, 150}, // C5
  {659, 200}, // E5
};

// clean + modern
static const Note STARTUP_3[] = {
  {494, 120}, // B4
  {587, 120}, // D5
  {659, 160}, // E5
  {784, 180}, // G5
  {0, 60},

  {659, 150},  // E5
  {784, 180},  // G5
  {988, 200},  // B5
  {1046, 240}, // C6
};

// Bright ascending triad → quick confirmation blip.
static const Note SUCCESS[] = {
  {523, 90},   // C5
  {659, 90},   // E5
  {784, 140},  // G5
  {1046, 160}, // C6
};

// Short, bouncy upward motion with a tiny flourish.
static const Note SUCCESS_2[] = {
  {659, 100},  // E5
  {784, 100},  // G5
  {988, 140},  // B5
  {1175, 180}, // D6
};

// A more melodic flourish that still stays compact.
static const Note SUCCESS_3[] = {
  {523, 100},  // C5
  {659, 120},  // E5
  {784, 120},  // G5
  {988, 100},  // B5
  {1319, 180}, // E6 (bright twirl)
};

// Short, stern downward tones.
static const Note ERROR[] = {
  {392, 160}, // G4
  {330, 160}, // E4
  {262, 220}, // C4
  {0, 120},   // brief rest
  {196, 260}, // G3 (low "bonk")
};

// Tense descending motif, short but very clear.
static const Note ERROR_2[] = {
  {784, 160}, // G5
  {659, 160}, // E5
  {523, 220}, // C5
  {0, 60},
  {392, 260}, // G4 (final drop)
};

// A glitch-like pattern followed by a low thud.
static const Note ERROR_3[] = {
  {659, 120}, // E5
  {0, 40},
  {659, 120}, // E5 (stutter)
  {0, 60},
  {494, 160}, // B4
  {440, 200}, // A4
  {392, 260}, // G4 (low drop)
};

// urgent beeps
static const Note WARNING[] = {
  {988, 120}, // B5
  {0, 80},
  {988, 120},
  {0, 80},
  {988, 200},
};

// A subtle escalating attention pattern.
static const Note WARNING_2[] = {
  {523, 120}, // C5
  {0, 60},
  {587, 140}, // D5
  {0, 60},
  {659, 160}, // E5
};

// Two short pings followed by a longer warning tone.
static const Note WARNING_3[] = {
  {659, 100}, // E5
  {0, 60},
  {659, 100}, // E5
  {0, 60},
  {784, 220}, // G5 (held warning)
};

// Think: “happy gadget booting up.”
static const Note CHEERFUL[] = {
  {523, 150}, // C5
  {659, 150}, // E5
  {784, 180}, // G5
  {988, 180}, // B5

  {1046, 220}, // C6
  {988, 160},  // B5
  {784, 160},  // G5
  {659, 180},  // E5

  {784, 200},  // G5
  {988, 200},  // B5
  {1175, 260}, // D6
  {1319, 320}, // E6

  {0, 80},     // pause
  {1175, 250}, // D6
  {988, 200},  // B5
  {784, 220},  // G5
};

// Triumphant + adventurous, without matching any copyrighted tune.
static const Note ADVENTURE[] = {
  {392, 120}, // G4
  {523, 160}, // C5
  {659, 160}, // E5
  {784, 200}, // G5

  {880, 200},  // A5
  {988, 220},  // B5
  {1046, 260}, // C6
  {0, 80},     // rest

  {1046, 200}, // C6
  {1175, 200}, // D6
  {1319, 260}, // E6
  {0, 60},

  {1319, 220}, // E6
  {1175, 200}, // D6
  {988, 200},  // B5
  {784, 260},  // G5
};

// Think “smart appliance with personality.”
static const Note PLAYFUL[] = {
  {659, 120}, // E5
  {784, 120}, // G5
  {880, 150}, // A5
  {988, 150}, // B5

  {880, 120}, // A5
  {784, 120}, // G5
  {659, 150}, // E5
  {523, 180}, // C5

  {659, 160},  // E5
  {784, 160},  // G5
  {988, 180},  // B5
  {1175, 260}, // D6

  {0, 80},
  {1175, 240}, // D6
  {988, 180},  // B5
  {784, 200},  // G5
};

// Folk-ish, rolling like waves.
static const Note SHANTY[] = {
  {392, 200}, // G4
  {440, 200}, // A4
  {494, 250}, // B4
  {523, 250}, // C5

  {494, 200}, // B4
  {440, 200}, // A4
  {392, 250}, // G4
  {330, 280}, // E4

  {392, 200}, // G4
  {494, 200}, // B4
  {523, 250}, // C5
  {587, 300}, // D5

  {0, 80},
  {523, 240}, // C5
  {494, 220}, // B4
  {440, 260}, // A4
};

static const Note JINGLE[] = {
  {784, 150},  // G5
  {880, 150},  // A5
  {988, 180},  // B5
  {784, 150},  // G5
  {988, 180},  // B5
  {1175, 250}, // D6
  {0, 80},
  {1175, 300}, // D6 sustain
};

// Triumphant, fast, bright.
static const Note VICTORY[] = {
  {659, 100},  // E5
  {784, 100},  // G5
  {988, 160},  // B5
  {1319, 220}, // E6
  {0, 60},
  {988, 160},  // B5
  {1175, 220}, // D6
};

// Great for easter eggs or playful UI interactions.
static const Note GOOFY[] = {
  {523, 120}, // C5
  {659, 120}, // E5
  {392, 180}, // G4
  {0, 70},
  {784, 200}, // G5
};

// Heavy wobble pattern + a mini drop.
static const Note DUBSTEP[] = {
  // Intro stab
  {1046, 120}, // C6
  {1175, 120}, // D6
  {1319, 180}, // E6
  {0, 60},

  // Wub-wub-wub pattern (fast gated notes)
  {392, 90}, // G4
  {0, 40},
  {392, 90},
  {0, 40},
  {392, 120},
  {0, 80},

  // Wobble pitch glide 1 (descending)
  {523, 90}, // C5
  {494, 90},
  {440, 90},
  {392, 120},

  // Wub-wub again but lower
  {349, 90}, // F4
  {0, 40},
  {349, 90},
  {0, 40},
  {349, 120},
  {0, 80},

  // Build-up rising pitch
  {440, 90},  // A4
  {523, 90},  // C5
  {587, 100}, // D5
  {659, 120}, // E5
  {784, 140}, // G5
  {988, 160}, // B5

  // DROP!
  {196, 200}, // G3 (sub buzz)
  {0, 60},
  {220, 200}, // A3
  {0, 60},
  {196, 260}, // G3
  {0, 100},

  // Final punchy stabs
  {523, 150}, // C5
  {392, 150}, // G4
  {659, 200}, // E5
};

// Feels like a melodic EDM motif with tasteful wobble.
static const Note DUBSTEP_MELODIC[] = {
  // ==== Melodic Hook ====
  {523, 150}, // C5
  {659, 150}, // E5
  {784, 180}, // G5
  {988, 180}, // B5
  {880, 150}, // A5
  {784, 160}, // G5
  {659, 180}, // E5
  {523, 200}, // C5
  {0, 60},

  // ==== Melodic Wub Pattern ====
  {440, 100}, // A4 (wub 1)
  {0, 40},
  {440, 100}, // A4
  {0, 40},
  {494, 120}, // B4
  {0, 50},

  {523, 130}, // C5 (lift)
  {0, 40},
  {587, 150}, // D5
  {0, 50},

  // ==== Mini Glide Down (EDM style) ====
  {659, 120}, // E5
  {622, 120}, // Eb5
  {587, 130}, // D5
  {523, 150}, // C5
  {0, 70},

  // ==== Build-up (melodic rising arpeggio) ====
  {523, 100},  // C5
  {659, 100},  // E5
  {784, 120},  // G5
  {988, 130},  // B5
  {1175, 140}, // D6
  {1319, 200}, // E6
  {0, 80},

  // ==== Melodic Drop ====
  {392, 150}, // G4 (warm sub-like)
  {0, 40},
  {440, 150}, // A4
  {0, 40},
  {494, 200}, // B4
  {0, 40},

  {784, 240}, // G5 (drop peak)
  {659, 220}, // E5
  {523, 260}, // C5
};

// Beethoven – “Ode to Joy”
static const Note ODE_TO_JOY[] = {
  {659, 750},
  {0, 30},
  {659, 750},
  {698, 750},
  {784, 750},
  {784, 750},
  {0, 30},
  {698, 750},
  {659, 750},
  {587, 750},

  {523, 750},
  {0, 30},
  {523, 750},
  {587, 750},
  {659, 750},
  {659, 1125},
  {0, 30},
  {587, 375},
  {587, 1500},
  {0, 187},

  {659, 750},
  {0, 30},
  {659, 750},
  {698, 750},
  {784, 750},
  {784, 750},
  {0, 30},
  {698, 750},
  {659, 750},
  {587, 750},

  {523, 750},
  {0, 30},
  {523, 750},
  {587, 750},
  {659, 750},
  {587, 1125},
  {523, 1125},
};

// Bach – “Minuet in G”
static const Note MINUET_IN_G[] = {
  {784, 535},
  {659, 535},
  {698, 535},
  {784, 535},
  {659, 535},
  {523, 1070}, // half note
  {587, 535},
  {659, 1070},

  {698, 535},
  {587, 535},
  {659, 535},
  {698, 535},
  {587, 535},
  {494, 1070},
  {523, 535},
  {587, 1070},

  {659, 535},
  {523, 535},
  {587, 535},
  {659, 535},
  {523, 535},
  {440, 1070},
  {494, 535},
  {523, 1070},
};

// Mozart – “Eine Kleine Nachtmusik”
static const Note NACHTMUSIK[] = {
  {988, 500},
  {1175, 500},
  {1319, 750}, // dotted quarter = 750
  {1175, 500},
  {988, 750},
  {784, 750}, // dotted quarters
  {880, 500},
  {988, 1000}, // half note

  {880, 500},
  {988, 500},
  {1046, 750},
  {988, 500},
  {880, 750},
  {698, 750},
  {784, 500},
  {880, 1000},
};

// Grieg – “In the Hall of the Mountain King”
static const Note MOUNTAIN_KING[] = {
  {392, 577},
  {440, 577},
  {392, 577},
  {440, 577},
  {392, 577},
  {440, 577},
  {494, 1154},

  {392, 577},
  {440, 577},
  {392, 577},
  {440, 577},
  {494, 577},
  {523, 577},
  {587, 1154},
};

// Tchaikovsky – “Dance of the Sugar Plum Fairy”
static const Note SUGAR_PLUM[] = {
  {784, 227},
  {740, 227},
  {784, 227},
  {988, 455},
  {880, 227},
  {740, 227},
  {880, 227},
  {1046, 683},
  {988, 455},
  {784, 227},
  {880, 227},
  {740, 455},
  {0, 150},

  {784, 227},
  {740, 227},
  {784, 227},
  {988, 455},
  {880, 227},
  {740, 227},
  {880, 227},
  {1046, 683},
  {988, 455},
  {880, 455},
  {784, 455},
  {740, 683},
};

// Handel – “Hallelujah Chorus”
static const Note HALLELUJAH[] = {
  {523, 600},
  {0, 30},
  {523, 600},
  {587, 600},
  {523, 600},
  {659, 900},
  {587, 600},
  {523, 1200},
  {0, 150},

  {659, 600},
  {0, 30},
  {659, 600},
  {698, 600},
  {659, 600},
  {784, 900},
  {698, 600},
  {659, 1200},
};

// Brahms – Lullaby (opening phrase)
static const Note LULLABY[] = {
  {392, 833},
  {0, 30},
  {392, 833},
  {523, 833},
  {0, 30},
  {523, 833},
  {587, 1666},
  {0, 30},
  {587, 1666},
  {523, 1666},
  {0, 200},

  {392, 833},
  {0, 30},
  {392, 833},
  {523, 833},
  {0, 30},
  {523, 833},
  {587, 1666},
  {0, 30},
  {587, 1666},
  {523, 1666},
  {0, 200},

  {587, 833},
  {0, 30},
  {587, 833},
  {659, 833},
  {0, 30},
  {659, 833},
  {698, 1666},
  {0, 30},
  {698, 1666},
  {659, 1666},
  {0, 200},

  {587, 833},
  {0, 30},
  {587, 833},
  {659, 833},
  {0, 30},
  {659, 833},
  {698, 1666},
  {0, 30},
  {698, 1666},
  {659, 1666},
};

// Example melody (3 beeps)
static const Note ACTIVE_STARTUP[] = {
  {3000, 50},
  {0, 50},
  {3000, 50},
  {0, 50},
  {3000, 50},
};

// Short–short–long.
static const Note ACTIVE_SUCCESS[] = {
  {3000, 120},
  {0, 50},
  {3000, 120},
  {0, 50},
  {3000, 250},
};

// Long, short, long with pauses.
static const Note ACTIVE_ERROR[] = {
  {3000, 250},
  {0, 120},
  {3000, 120},
  {0, 120},
  {3000, 350},
};

// Three fast pulses.
static const Note ACTIVE_WARNING[] = {
  {3000, 120},
  {0, 80},
  {3000, 120},
  {0, 80},
  {3000, 120},
};

static const Melody melodyTable[] = {
  #ifdef YB_PIEZO_PASSIVE
  MELODY_ENTRY(STARTUP),
  MELODY_ENTRY(STARTUP_2),
  MELODY_ENTRY(STARTUP_3),
  MELODY_ENTRY(SUCCESS),
  MELODY_ENTRY(SUCCESS_2),
  MELODY_ENTRY(SUCCESS_3),
  MELODY_ENTRY(ERROR),
  MELODY_ENTRY(ERROR_2),
  MELODY_ENTRY(ERROR_3),
  MELODY_ENTRY(WARNING),
  MELODY_ENTRY(WARNING_2),
  MELODY_ENTRY(WARNING_3),
  MELODY_ENTRY(CHEERFUL),
  MELODY_ENTRY(ADVENTURE),
  MELODY_ENTRY(PLAYFUL),
  MELODY_ENTRY(SHANTY),
  MELODY_ENTRY(JINGLE),
  MELODY_ENTRY(VICTORY),
  MELODY_ENTRY(GOOFY),
  MELODY_ENTRY(DUBSTEP),
  MELODY_ENTRY(DUBSTEP_MELODIC),
  MELODY_ENTRY(ODE_TO_JOY),
  MELODY_ENTRY(MINUET_IN_G),
  MELODY_ENTRY(NACHTMUSIK),
  MELODY_ENTRY(MOUNTAIN_KING),
  MELODY_ENTRY(SUGAR_PLUM),
  MELODY_ENTRY(HALLELUJAH),
  MELODY_ENTRY(LULLABY),
  #endif
  #ifdef YB_PIEZO_ACTIVE
  MELODY_ENTRY(ACTIVE_STARTUP),
  MELODY_ENTRY(ACTIVE_SUCCESS),
  MELODY_ENTRY(ACTIVE_ERROR),
  MELODY_ENTRY(ACTIVE_WARNING),
  #endif
};
static const size_t melodyCount = sizeof(melodyTable) / sizeof(melodyTable[0]);
#endif