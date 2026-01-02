## v2.0

* add buzzer and rgb to the example
* setup firmware release scripts

## Long Term

* get espwebtools integrated for first firmware upload
  * modify release script to add firmwares to the release script.
  * keep all previous firmwares for rollback potential

* other app.* callbacks to register various things?
  * consolidate config and settings
    * addSettings() similar to addPage() but with settings content.

* bug with reconnecting (eg ESP.restart not going down the hello path)

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer