// IntervalTimer.h
#pragma once
#include "YarrboardDebug.h"
#include <Arduino.h>
#include <cstring>
#include <stdint.h>
#include <vector>

/**
 * IntervalTimer
 *  - Call start() once to set the initial timestamp.
 *  - Wrap code blocks with time("label") to record the elapsed microseconds since the last mark.
 *  - Call print(interval_ms) repeatedly; it prints only when interval_ms has elapsed since the last print.
 *
 * Notes:
 *  - Uses micros() for timing and uint32_t subtraction for rollover-safe intervals.
 *  - Stores per-label totals and counts in a std::vector.
 *  - Labels should be stable C strings (e.g., string literals) for the lifetime of the program.
 */
class IntervalTimer
{
  public:
    IntervalTimer() : _last_us(0), _last_print_ms(0) {}

    // Mark the starting point for the next interval.
    void start() { _last_us = micros(); }

    // Record elapsed time since the most recent start()/time() and attribute it to `label`.
    void time(const char* label)
    {
      const uint32_t now = micros();
      const uint32_t delta = now - _last_us; // rollover-safe with unsigned math
      _last_us = now;

      Entry& e = findOrCreate(label);
      e.total_us += static_cast<uint64_t>(delta);
      e.count += 1;
    }

    // Clear all recorded stats and reset the last timestamp.
    void reset()
    {
      _entries.clear();
      _last_us = micros();
    }

    // Print averages for each label.
    // - interval_ms == 0 => print immediately.
    // - otherwise prints only if at least interval_ms has elapsed since last print.
    void print(uint32_t interval_ms = 0)
    {
      const uint32_t now_ms = millis();
      if (interval_ms != 0) {
        const uint32_t elapsed = now_ms - _last_print_ms; // rollover-safe
        if (elapsed < interval_ms)
          return;
      }
      _last_print_ms = now_ms;

      if (_entries.empty())
        return;

      unsigned long total_us = 0;
      YBP.println(F("=== IntervalTimer averages (us) ==="));
      for (const auto& e : _entries) {
        if (e.count == 0)
          continue;
        const uint32_t avg_us = static_cast<uint32_t>(e.total_us / e.count);
        total_us += avg_us;
        // Keep it simple: label, average in microseconds, and sample count.
        YBP.printf("%s: avg=%lu us  (n=%lu)\n",
          e.label ? e.label : "(null)",
          static_cast<unsigned long>(avg_us),
          static_cast<unsigned long>(e.count));
      }
      YBP.printf("Total: avg=%lu us\n",
        static_cast<unsigned long>(total_us));
    }

  private:
    struct Entry {
        const char* label; // expected to be a stable C-string (e.g., literal)
        uint64_t total_us; // sum of intervals in microseconds
        uint32_t count;    // number of intervals recorded
    };

    std::vector<Entry> _entries;
    uint32_t _last_us;       // last timestamp from start()/time(), in micros()
    uint32_t _last_print_ms; // last time we printed, in millis()

    Entry& findOrCreate(const char* label)
    {
      for (auto& e : _entries) {
        if ((e.label == label) || (e.label && label && std::strcmp(e.label, label) == 0)) {
          return e;
        }
      }
      _entries.push_back({label, 0ULL, 0U});
      return _entries.back();
    }
};
