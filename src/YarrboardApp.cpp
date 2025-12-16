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

#include "ntp.h"

YarrboardApp::YarrboardApp() : config(*this),
                               network(*this, config),
                               http(*this, config),
                               protocol(*this, config),
                               auth(*this, config),
                               navico(*this, config),
                               mqtt(*this, config),
                               ota(*this, config),
                               rgb(*this, config),
                               buzzer(*this, config),
                               networkLogger(protocol),
                               loopSpeed(100, 1000),
                               framerateAvg(10, 10000)

{
}

void YarrboardApp::setup()
{
  debug_setup();

  // get our prefs early on.
  config.setup();

  rgb.setup();
  YBP.println("RGB ok");

  buzzer.setup();
  YBP.println("Buzzer ok");

  network.setup();
  YBP.println("Network ok");

  ntp_setup();
  YBP.println("NTP ok");

  http.setup();
  YBP.println("Server ok");

  protocol.setup();
  YBP.println("Protocol ok");

  auth.setup();
  YBP.println("Auth ok");

  ota.setup();
  YBP.println("OTA ok");

#ifdef YB_HAS_BUS_VOLTAGE
  bus_voltage_setup();
  YBP.println("Bus voltage ok");
#endif

#ifdef YB_HAS_ADC_CHANNELS
  adc_channels_setup();
  YBP.println("ADC channels ok");
#endif

#ifdef YB_HAS_PWM_CHANNELS
  pwm_channels_setup();
  YBP.println("PWM channels ok");
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  relay_channels_setup();
  YBP.println("Relay channels ok");
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  servo_channels_setup();
  YBP.println("Servo channels ok");
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  stepper_channels_setup();
  YBP.println("Stepper channels ok");
#endif

#ifdef YB_HAS_FANS
  fans_setup();
  YBP.println("Fans ok");
#endif

#ifdef YB_IS_BRINEOMATIC
  brineomatic_setup();
#endif

  // we need to do this last so that all our channels, etc are fully configured.
  mqtt.setup();
  YBP.println("MQTT ok");

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

  network.loop();
  it.time("network_loop");

  rgb.loop();
  it.time("rgb_loop");

  buzzer.loop();
  it.time("buzzer_loop");

#ifdef YB_HAS_ADC_CHANNELS
  adc_channels_loop();
  it.time("adc_loop");
#endif

#ifdef YB_HAS_PWM_CHANNELS
  pwm_channels_loop();
  it.time("pwm_loop");
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  relay_channels_loop();
  it.time("relay_loop");
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  servo_channels_loop();
  it.time("servo_loop");
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  stepper_channels_loop();
  it.time("stepper_loop");
#endif

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

  network.loop();
  it.time("network_loop");

  http.loop();
  it.time("http_loop");

  protocol.loop();
  it.time("protocol_loop");

  auth.loop();
  it.time("auth_loop");

  mqtt.loop();
  it.time("mqtt_loop");

  ota.loop();
  it.time("ota_loop");

  if (config.app_enable_mfd) {
    navico.loop();
    it.time("navico_loop");
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