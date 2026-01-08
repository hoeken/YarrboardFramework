## Long Term

* add static ip address (yarrboard-firmware #11)
* add compile targets for each board type to firmware releases
* add minimum flash size (8mb) + talk about A/B partition for OTA

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer