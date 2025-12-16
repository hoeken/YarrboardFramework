## v1.0.0 Release

* modify the gulp.js script to pull from the framework lib directory
* project specific gulp rules to pull in custom stuff logo, css, js, etc.
    * html/index.html -> overrides framework html
    * html/logo.png -> overrides framework.html
    * html/css/* -> gets added to the framework css includes
    * html/js/* -> gets added to the framework js includes
* clean up the app.setup and app.loop calls (no more full_*)
* move scattered auth stuff into an AuthController class
* BaseControllers refactor
  * setup, loop, end
  * static _instance
  * pass in _app and _config
  * yba.registerController("name", controller);
* copy the ChannelRegistry style from JS and port to c++
  * move channel headers/classes to a channels/ folder
  * rename foo_channel.cpp to FooChannel.cpp to match class names
  * create callbacks:
      * onSetup() - main.cpp
      * onLoop() - main.cpp
      * onInit() - prefs.cpp
      * onGenerateConfig() - prefs.cpp
      * onLoadConfig() - prefs.cpp
      * onMQTTUpdate() - mqtt.cpp
      * onHADiscovery() - mqtt.cpp
      * onUpdate() - protocol.cpp
      * onFastUpdate() - protocol.cpp
      * onStats() - protocol.cpp
      * onMessage() - gets called for every single message
      * registerNobodyCommand("command", callback);
      * registerGuestCommand("command", callback);
      * registerAdminCommand("command", callback);
  * for looping only stuff it would be best to do something like this:
      * yba.registerChannels(channels);
      * yba.registerMQTTChannels(channels);
      * yba.registerHAChannels(channels);
* figure out how to manage BaseChannel mqtt calls -> really dont want to have to pass config + app details to them.
* figure out how to properly handle USB_CDC mode to avoid double prints.

* fix crash after Improv success (probably need to call setup() again)
Backtrace: 0x40381935:0x3fcebdd0 0x403818fd:0x3fcebdf0 0x403889fe:0x3fcebe10 0x40382246:0x3fcebf50 0x420346f5:0x3fcebf90 0x4203a7ec:0x3fcebfc0 0x4203aece:0x3fcebfe0 0x420437c0:0x3fcec000 0x403827f1:0x3fcec020
  #0  0x40381935 in panic_abort at /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/panic.c:477
  #1  0x403818fd in esp_system_abort at /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/esp_system/port/esp_system_chip.c:87
  #2  0x403889fe in __assert_func at /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/newlib/src/assert.c:80
  #3  0x40382246 in xQueueReceive at /home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/freertos/FreeRTOS-Kernel/queue.c:1535 (discriminator 2)
  #4  0x420346f5 in HTTPController::loop() at src/HTTPController.cpp:219