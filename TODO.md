## v1.1

* is_first_boot / improvwifi
  * if yarrboard.json is found -> also check config.is_first_boot so its possible to bundle a default json
  * if wifi does not connect, we should wait for a 5s press of boot, re-enable is_first_boot (or new variable like improve_finished = false)

* bootstrap nav bar seems to be slightly broken
  * nav bar not working well on vertical mobile

* global brightness
  * rgb controller -> setMaxBrightness()
  * onBrightness -> max * brightness

* we need to add an order field to controllers or registerController() that determines how the setup / loop order.
  * sort on add
  * mqtt needs to be last

* login, hello, logout are special commands.  would be nice to have them use the same command registry
  * problem is we need certain context specific things (mode, connection id, and role)
  * currently we have a chance of collisions if http api + websockets requests happen simulataneously.
    * best to solve this as a mutex
  * need to modify the protocol controller callback definition to include a context struct.
    * struct contains mode, connection id, and role
  * we will need this to implement protocol over mqtt
  * current command entry flow:
    * SERIAL: main thread -> ProtocolController::loop -> serial -> handleReceivedJSON
    * WEBSOCKET: main thread -> HTTPController::loop -> handleWebsocketMessageLoop -> handleReceivedJSON
    * HTTP API: esp-idf HTTP thread -> handleWebServerRequest -> handleReceivedJSON
    * MQTT: not implemented
  * problem with a single entry handler is that we want to route the responses back to the right interface
    * each interface set its own context and then calls handleReceivedJSON to process it

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