## v2.0

* addMessageHandler -> onMessage (homogenize)
* onMessage -> messageHandler (homogenize)
* rename control to home

* startUpdateData / statsdata
  * homogenize to startUpdatePoller / startStatsPoller
  * add stopUpdatePoller, etc to control.onClose, stats, etc.

* move SendIt/adc stuff to sendit

* add + icons to the expandable info on stats.

## Long Term

* other app.* callbacks to register various things?

* bug with reconnecting (eg ESP.restart not going down the hello path)

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer