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

// RollingAverage.h
#pragma once
#include <Arduino.h>

#ifndef RA_DEFAULT_SIZE
  #define RA_DEFAULT_SIZE 50
#endif

#ifndef RA_DEFAULT_WINDOW
  #define RA_DEFAULT_WINDOW 1000
#endif

/**
 * @brief RollingAverage maintains a running average of recent samples
 *        collected within a given time window (in milliseconds).
 *
 * The class stores up to 'capacity' samples in a ring buffer. When new
 * samples are added, old ones that fall outside the specified window
 * (based on millis()) are automatically discarded.
 *
 * Example:
 *   RollingAverage ra(128, 1000);  // 128-sample buffer, 1-second window
 *   ra.add(analogRead(A0));
 *   unsigned int avg = ra.average();       // get average of last 1s of data
 */
class RollingAverage
{
  public:
    /**
     * @param capacity  Maximum number of stored samples.
     * @param window_ms Time window in milliseconds over which to average.
     */
    RollingAverage(uint16_t capacity, uint32_t window_ms = 1000)
        : cap_(capacity ? capacity : 1), window_(window_ms)
    {
      buf_ = new Sample[cap_];
    }

    /** Destructor — frees allocated memory. */
    ~RollingAverage() { delete[] buf_; }

    /** Clear all stored samples and reset state. */
    inline void clear()
    {
      head_ = tail_ = count_ = 0;
      sum_ = 0;
    }

    /**
     * @brief Add a new sample value.
     *
     * This automatically discards any samples older than window_ms.
     * If the buffer is full, the oldest sample is overwritten.
     *
     * @param v The sample value to add.
     */
    inline void add(uint32_t v)
    {
      const uint32_t now = millis();
      prune(now);

      // Drop oldest if buffer full
      if (count_ == cap_) {
        sum_ -= buf_[head_].v;
        head_ = next(head_);
        --count_;
      }

      buf_[tail_] = {v, now};
      tail_ = next(tail_);
      ++count_;
      sum_ += v;
    }

    /**
     * @brief Compute the current average of samples within the time window.
     *
     * @param fast If true (default), use the precomputed running sum (fast).
     *             If false, recalculate the sum from scratch (slower but accurate
     *             if you've modified data manually or want to verify integrity).
     * @return The average value, or 0. if no valid samples exist.
     */
    inline uint32_t average(bool fast = true)
    {
      prune(millis());
      if (!count_)
        return 0;

      if (fast) {
        return sum_ / count_;
      } else {
        uint32_t total = 0;
        for (uint16_t i = 0, idx = head_; i < count_; ++i) {
          total += buf_[idx].v;
          idx = next(idx);
        }
        return total / count_;
      }
    }

    /**
     * @brief Get the most recent sample value.
     *
     * @return The latest value, or 0 if no samples exist.
     */
    inline uint32_t latest()
    {
      prune(millis());
      if (!count_)
        return 0;
      const uint16_t last = (tail_ == 0) ? (cap_ - 1) : (tail_ - 1);
      return buf_[last].v;
    }

    /**
     * @brief Get the number of valid samples currently inside the time window.
     */
    inline uint16_t count()
    {
      prune(millis());
      return count_;
    }

    /**
     * @brief Get the capacity of total samples.
     */
    inline uint16_t cap()
    {
      return cap_;
    }

    /**
     * @brief Get the window in ms
     */
    inline uint32_t window()
    {
      return window_;
    }
    /**
     * @brief Set the window in ms
     */
    inline void setWindow(uint32_t window_ms)
    {
      window_ = window_ms;
      clear(); // existing samples used wrong window, reset for clean state
    }

    /**
     * @brief Get the sample value at a specific index.
     *
     * Index 0 refers to the oldest sample, and index (count-1) refers to the newest.
     *
     * @param i The index of the sample to retrieve (0 to count-1).
     * @return The sample value at index i, or 0 if index is out of range.
     */
    inline uint32_t get(uint16_t i)
    {
      prune(millis());
      if (i >= count_)
        return 0;
      const uint16_t idx = (head_ + i) % cap_;
      return buf_[idx].v;
    }

  private:
    struct Sample {
        uint32_t v; ///< Sample value
        uint32_t t; ///< Timestamp in milliseconds
    };

    Sample* buf_ = nullptr;
    uint16_t cap_ = 0;    ///< Max samples
    uint16_t head_ = 0;   ///< Index of oldest sample
    uint16_t tail_ = 0;   ///< Index for next write
    uint16_t count_ = 0;  ///< Current sample count
    uint32_t sum_ = 0;    ///< Running sum for fast average
    uint32_t window_ = 0; ///< Time window in ms

    /** @brief Get the next index in the ring buffer, wrapping to 0 at capacity. */
    inline uint16_t next(uint16_t i) const { return (i + 1u == cap_) ? 0u : (i + 1u); }

    /**
     * @brief Remove samples older than the time window.
     *
     * Uses unsigned subtraction so millis() wraparound is handled correctly.
     */
    inline void prune(uint32_t now)
    {
      while (count_) {
        const Sample& s = buf_[head_];
        if ((uint32_t)(now - s.t) <= window_)
          break;
        sum_ -= s.v;
        head_ = next(head_);
        --count_;
      }
    }
};