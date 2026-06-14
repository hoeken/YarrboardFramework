## v3.1

* add current version to System -> "Firmware is up to date"
* copy button nav style from brineomatic graphs to "Settings" tab
* firmware latest version bug
  * sometimes triggers when starting OTA, problem in espFOTA library
  * usually solved with a reboot
* Add client side Controllers
  * refactor App.js into smaller classes
  * save{Controller}Config()
  * form generation
  * form data setting
  * form data loading

## Long Term

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer