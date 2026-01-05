## v2.1

* consolidate config and settings
  * create addSettingsPanel() similar to addPage() but with settings content.
  * very basic - just name, title, and content

* get espwebtools integrated for first firmware upload
  * add improv wifi to firmware upload page.

* bug with reconnecting (eg ESP.restart not going down the hello path)
  * all queued messages should wait until the connection is authenticated and ready
  * get_stats / get_update should not jam the queue

## Long Term

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer