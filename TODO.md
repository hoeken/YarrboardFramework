## v3.0.0

* update FrothFET and SendIt with new framework
  * javascript
  * psram setup

* update documentation

## v3.1.0

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