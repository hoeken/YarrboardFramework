## v2.1

* consolidate config and settings
  * create addSettingsPanel() similar to addPage() but with settings content.
  * very basic - just name, title, and content

## Long Term

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer