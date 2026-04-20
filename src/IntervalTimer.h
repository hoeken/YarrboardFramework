/*
 * Yarrboard Framework
 *
 * Copyright (c) 2025 Zach Hoeken <hoeken@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

// IntervalTimer.h
#pragma once
#include "YarrboardDebug.h"
#include <Arduino.h>
#include <cstring>
#include <stdint.h>
#include <vector>

/**
 * IntervalTimer
 * * A lightweight profiling tool for Arduino to measure and average execution time of
 * code blocks using microsecond precision.
 * * Usage:
 * - Initialize with a Print object (e.g., IntervalTimer timer(Serial) or timer(YBP)).
 * - Call start() once to set the baseline timestamp.
 * - Call time("label") at the end of a code block to record micros since the last mark.
 * - Call print() to output a summary table of averages to the configured Print device.
 * - Call getEntries() to retrieve raw data for custom processing or JSON serialization.
 *
 * Technical Notes:
 * - Rollover-Safe: Uses uint32_t subtraction with micros() to handle hardware timer wrap-around.
 * - Memory: Stores results in a std::vector. Labels should be stable C-strings (string literals)
 * to avoid memory corruption or excessive string comparison.
 * - Flexibility: Output can be redirected to any Arduino 'Print' child class (Serial, File, LCD).
 */
class IntervalTimer
{
  public:
    struct Entry {
        const char* label; // expected to be a stable C-string (e.g., literal)
        uint64_t total_us; // sum of intervals in microseconds
        uint32_t count;    // number of intervals recorded
    };

    // Constructor now accepts a Print object, defaulting to Serial
    IntervalTimer(Print& printer = Serial) : _printer(&printer), _last_us(micros()) {}

    // Allow changing the printer at runtime if needed
    void setPrinter(Print& printer) { _printer = &printer; }

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

    const std::vector<Entry>& getEntries() const
    {
      return _entries;
    }

    // Print averages for each label.
    void print()
    {
      if (_entries.empty())
        return;

      unsigned long total_us = 0;
      _printer->println(F("=== IntervalTimer averages (us) ==="));
      for (const auto& e : _entries) {
        if (e.count == 0)
          continue;
        const uint32_t avg_us = static_cast<uint32_t>(e.total_us / e.count);
        total_us += avg_us;
        // Keep it simple: label, average in microseconds, and sample count.
        _printer->printf("%s: avg=%lu us  (n=%lu)\n",
          e.label ? e.label : "(null)",
          static_cast<unsigned long>(avg_us),
          static_cast<unsigned long>(e.count));
      }
      _printer->printf("Total: avg=%lu us\n",
        static_cast<unsigned long>(total_us));
    }

  private:
    Print* _printer; // Pointer to the output stream
    std::vector<Entry> _entries;
    uint32_t _last_us; // last timestamp from start()/time(), in micros()

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
