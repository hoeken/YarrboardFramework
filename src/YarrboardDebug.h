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

#pragma once

#include "controllers/ProtocolController.h"
#include "etl/vector.h"
#include <Arduino.h>

// Sets ArduinoTrace output to YBP so TRACE() macros go through the fan-out printer.
#define ARDUINOTRACE_SERIAL YBP
#include <ArduinoTrace.h>

#ifdef YB_USB_SERIAL
  #include "USB.h"
#endif

class YarrboardPrint : public Print
{
  public:
    YarrboardPrint() {}

    void addPrinter(Print& printer)
    {
      if (_printers.full() == false) {
        _printers.push_back(&printer);
      }
    }

    void removePrinter(Print& printer)
    {
      for (auto it = _printers.begin(); it != _printers.end(); ++it) {
        if (*it == &printer) {
          _printers.erase(it);
          break; // done
        }
      }
    }
    size_t write(uint8_t b) override
    {
      for (auto* p : _printers) {
        p->write(b);
      }
      return 1;
    }

  private:
    static constexpr size_t MAX_PRINTERS = 8; // tweak as needed
    etl::vector<Print*, MAX_PRINTERS> _printers;
};

class StringPrint : public Print
{
  public:
    StringPrint() : _pos(0) {}

    size_t write(uint8_t b) override
    {
      if (_pos < sizeof(_buf) - 1) {
        _buf[_pos++] = (char)b;
        _buf[_pos] = '\0';
        return 1;
      }
      return 0; // buffer full
    }

    const char* c_str() const { return _buf; }
    void reset() { _pos = 0; _buf[0] = '\0'; }

  private:
    static constexpr size_t BUF_SIZE = 2048;
    char _buf[BUF_SIZE];
    size_t _pos;
};

class WebsocketPrint : public Print
{
  public:
    WebsocketPrint(ProtocolController& protocol) : _proto(protocol), _pos(0) {}

    size_t write(uint8_t b) override
    {
      if (b == '\n') {
        if (_pos > 0) {
          _buf[_pos] = '\0';
          _proto.sendDebug(_buf);
          _pos = 0; // reset buffer
        }
      } else {
        if (_pos < sizeof(_buf) - 1) {
          _buf[_pos++] = (char)b;
          return 1;
        }
        return 0; // buffer full
      }
      return 1;
    }

  private:
    ProtocolController& _proto;
    static constexpr size_t BUF_SIZE = 256;
    char _buf[BUF_SIZE];
    size_t _pos;
};

extern YarrboardPrint YBP;
extern StringPrint startupLogger;