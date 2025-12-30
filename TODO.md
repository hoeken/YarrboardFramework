## v1.1

* bug with reconnecting (eg ESP.restart not going down the hello path)

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* implement protocol over mqtt

* figure out how to compile without nimbleble -> move to external dependency

* more App.js functions:
  * addPage("page", "content", openCallback);
  * onOpenPage(page, callback);
  * other app.* callbacks to register various things?

## Long Term

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer