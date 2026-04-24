## v3.0.0

* rename app_enable_* to foo_enabled in json configs.
  * sanitize rename (or add/remove)
  * load+generate use new name
  * client ui use new name
* move YarrboardApp.enable_foo to controller.default_foo_enabled

* implement more sanitizeConfig() on different params
* more testing of everything:
  * shareable config
  * v1 config
  * v2 config
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