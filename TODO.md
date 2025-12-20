## v1.0.0 Release

* add github_url

* pull in latest html / js from yb-firmware

* create a README describing the project

## Long Term

* add a http server item to the stats sidebar (or possibly comms / protocol?)
  * move clients / messages / etc to that.

* modify the gulp.js script to pull from the framework lib directory
  * it should also write the files to the framework lib directory for inclusion
  * project specific gulp rules to pull in custom stuff logo, css, js, etc.
      * html/index.html -> overrides framework html
      * html/logo.png -> overrides framework.html
      * html/css/* -> gets added to the framework css includes
      * html/js/* -> gets added to the framework js includes

* we need to add an order field to controllers that determines how they loop.
  * sort on add.
  * mqtt needs to be last

* global brightness
  * rgb controller -> setMaxBrightness()
  * onBrightness -> max * brightness

* login, hello, logout are special commands.  can we modify this?
  * probably too much work for now.  implementing a queue similar to websockets would be the best way.
  * i guess there's a chance of collisions if there are http requests and websockets happening together.
  * command entry flow:
    * SERIAL: ProtocolController::loop -> serial -> handleReceivedJSON
    * WEBSOCKET: HTTPController::loop -> handleWebsocketMessageLoop -> handleReceivedJSON
    * HTTP API: esp-idf HTTP thread -> handleWebServerRequest -> handleReceivedJSON
    * MQTT: not implemented
