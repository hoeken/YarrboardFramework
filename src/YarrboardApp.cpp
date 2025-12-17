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

#include "YarrboardApp.h"
#include "YarrboardDebug.h"

YarrboardApp::YarrboardApp() : config(*this),
                               network(*this),
                               http(*this),
                               protocol(*this),
                               auth(*this),
                               mqtt(*this),
                               ota(*this),
                               rgb(*this),
                               buzzer(*this),
                               ntp(*this),
                               networkLogger(protocol),
                               loopSpeed(100, 1000),
                               framerateAvg(10, 10000)

{
  registerController(rgb);
  registerController(buzzer);
  registerController(network);
  registerController(ntp);
  registerController(http);
  registerController(protocol);
  registerController(auth);
  registerController(ota);
  registerController(mqtt);
}

void YarrboardApp::setup()
{
  debug_setup();

  YBP.println("Yarrboard");
  YBP.print("Hardware Version: ");
  YBP.println(hardware_version);
  YBP.print("Firmware Version: ");
  YBP.println(firmware_version);
  YBP.printf("Firmware build: %s (%s)\n", GIT_HASH, BUILD_TIME);
  YBP.print("Last Reset: ");
  YBP.println(getResetReason());

  // we need littlefs to store our coredump
  if (!LittleFS.begin(true)) {
    YBP.println("ERROR: Unable to mount LittleFS");
  }
  YBP.printf("LittleFS Storage: %d / %d\n", LittleFS.usedBytes(), LittleFS.totalBytes());

  // get our prefs early on.
  config.setup();

#ifdef YB_HAS_BUS_VOLTAGE
  bus_voltage_setup();
  YBP.println("Bus voltage ok");
#endif

#ifdef YB_IS_BRINEOMATIC
  brineomatic_setup();
#endif

  for (auto* c : _controllers) {
    if (c->setup())
      YBP.printf("✅ %s OK\n", c->getName());
    else
      YBP.printf("❌ %s FAILED\n", c->getName());
  }

  // we're done with startup log
  YBP.removePrinter(startupLogger);

  // network logger is a troublemaker
  YBP.addPrinter(networkLogger);

  lastLoopMicros = micros();
}

void YarrboardApp::loop()
{
  // start our interval timer
  it.start();

#ifdef YB_HAS_FANS
  fans_loop();
  it.time("fans_loop");
#endif

#ifdef YB_HAS_BUS_VOLTAGE
  bus_voltage_loop();
  it.time("bus_voltage_loop");
#endif

#ifdef YB_IS_BRINEOMATIC
  brineomatic_loop();
  it.time("brineomatic_loop");
#endif

  for (auto* c : _controllers) {
    c->loop();
    it.time(c->getName());
  }

  // our debug.
  // it.print(5000);

  // if (INTERVAL(1000))
  //   DUMP(millis());

  // calculate our framerate
  unsigned long loopDelta = micros() - lastLoopMicros;
  lastLoopMicros = micros();
  loopSpeed.add(loopDelta);
  unsigned long framerate_now = 1000000 / loopSpeed.average();

  // smooth out our frequency
  if (millis() - lastLoopMillis > 1000) {
    framerateAvg.add(framerate_now);
    framerate = framerateAvg.average();
    lastLoopMillis = millis();
  }
}

// Register a controller instance (non-owning).
// Returns false if full or name duplicate.
bool YarrboardApp::registerController(BaseController& controller)
{
  const char* n = controller.getName();
  if (!n || !*n)
    return false;

  if (getController(n) != nullptr) {
    // duplicate name
    return false;
  }

  if (_controllers.full()) {
    return false;
  }

  _controllers.push_back(&controller);
  return true;
}

// Lookup by name (nullptr if not found)
BaseController* YarrboardApp::getController(const char* name)
{
  if (!name || !*name)
    return nullptr;

  for (auto* c : _controllers) {
    if (c && c->getName() && (std::strcmp(c->getName(), name) == 0)) {
      return c;
    }
  }
  return nullptr;
}

const BaseController* YarrboardApp::getController(const char* name) const
{
  return const_cast<YarrboardApp*>(this)->getController(name);
}

// Remove by name (returns true if removed)
bool YarrboardApp::removeController(const char* name)
{
  if (!name || !*name)
    return false;

  for (size_t i = 0; i < _controllers.size(); i++) {
    BaseController* c = _controllers[i];
    if (c && c->getName() && (std::strcmp(c->getName(), name) == 0)) {
      _controllers.erase(_controllers.begin() + i);
      return true;
    }
  }
  return false;
}