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
                               debug(*this),
                               network(*this),
                               http(*this),
                               protocol(*this),
                               auth(*this),
                               mqtt(*this),
                               ota(*this),
                               ntp(*this),
                               networkLogger(protocol),
                               loopSpeed(100, 1000),
                               framerateAvg(10, 10000)

{
  registerController(debug, 10);
  registerController(config, 20);
  registerController(network, 30);
  registerController(ntp, 40);
  registerController(http, 50);
  registerController(protocol, 60);
  registerController(auth, 70);
  registerController(ota, 80);
  registerController(mqtt, 200);
}

void YarrboardApp::setup()
{
  for (const ControllerEntry& entry : _controllers) {
    if (entry.controller->start())
      YBP.printf("✅ %s setup OK\n", entry.controller->getName());
    else
      YBP.printf("❌ %s setup FAILED\n", entry.controller->getName());
  }

  // we're done with startup log
  YBP.removePrinter(startupLogger);

  YBP.printf("Startup complete.\n");

  // network logger is a troublemaker
  YBP.addPrinter(networkLogger);

  lastLoopMicros = micros();
}

void YarrboardApp::_handleImprov()
{
  // First boot mode?  That means we're doing ImprovWifi
  // we need to check improvDone because of race conditions.
  if (config.is_first_boot && !network.improvDone) {
    network.loop(); // Handle Improv serial communication
    return;
  }

  // after first boot is done, start the failed controllers
  if (network.improvDone) {
    // start our services now!
    network.startServices();

    // save our json config.
    char error[128];
    config.saveConfig(error, sizeof(error));

    YBP.println("Re-starting failed controllers.");
    for (const ControllerEntry& entry : _controllers) {
      if (!entry.controller->isStarted()) {
        if (entry.controller->start())
          YBP.printf("✅ %s setup OK\n", entry.controller->getName());
        else
          YBP.printf("❌ %s setup FAILED\n", entry.controller->getName());
      }
    }

    // we're totally done now.
    network.improvDone = false;
    YBP.println("First Boot Setup Complete.");
  }
}

void YarrboardApp::loop()
{
  // for first boot.
  if (config.is_first_boot || network.improvDone) {
    _handleImprov();
    return;
  }

  // start our interval timer
  debug.it.start();

  for (const ControllerEntry& entry : _controllers) {
    entry.controller->loop();
    debug.it.time(entry.controller->getName());
  }

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
// Controllers are sorted by order (lower values run first).
bool YarrboardApp::registerController(BaseController& controller, uint8_t order)
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

  // Create new entry
  ControllerEntry entry(&controller, order);

  // Find insertion point to maintain sorted order
  auto it = _controllers.begin();
  while (it != _controllers.end() && it->order <= order) {
    ++it;
  }

  // Insert at the correct position
  _controllers.insert(it, entry);
  return true;
}

// Lookup by name (nullptr if not found)
BaseController* YarrboardApp::getController(const char* name)
{
  if (!name || !*name)
    return nullptr;

  for (const ControllerEntry& entry : _controllers) {
    if (entry.controller && entry.controller->getName() && (std::strcmp(entry.controller->getName(), name) == 0)) {
      return entry.controller;
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
    const ControllerEntry& entry = _controllers[i];
    if (entry.controller && entry.controller->getName() && (std::strcmp(entry.controller->getName(), name) == 0)) {
      _controllers.erase(_controllers.begin() + i);
      return true;
    }
  }
  return false;
}

void YarrboardApp::setStatusColor(uint8_t r, uint8_t g, uint8_t b)
{
  RGBControllerInterface* rgb = (RGBControllerInterface*)getController("rgb");
  if (rgb)
    rgb->setStatusColor(r, g, b);
}

void YarrboardApp::setStatusColor(const CRGB& color)
{
  RGBControllerInterface* rgb = (RGBControllerInterface*)getController("rgb");
  if (rgb)
    rgb->setStatusColor(color);
}

void YarrboardApp::playMelody(const char* melody)
{
  BuzzerController* buzzer = (BuzzerController*)getController("buzzer");
  if (buzzer)
    buzzer->playMelodyByName(melody);
}