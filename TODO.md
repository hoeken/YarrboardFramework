## v3.0.0

* fix controllers to pass parameters as config object and then use sanitize -> load flow
  * ProtocolController::handleSetGeneralConfig (add test for melody exists)

* fix settings links to more documentation

* more testing of everything:
  * shareable config
  * delete random bits of config

* update FrothFET and SendIt with new framework
  * firmware calls
  * javascript
  * psram setup

## Long Term

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer