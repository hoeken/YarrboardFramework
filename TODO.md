## Refactors

* Asymmetric delegation — MODERATE
Board config uses hooks (loadConfigHook / generateConfigHook), but app config uses direct calls (loadAuthConfig(), generateOTAConfig(), etc.). Two different patterns for the same job creates confusion about which controllers need hooks and which don't.

* Error propagation hole — MODERATE
loadNetworkConfigFromJSON() can return false and propagate errors. loadAppConfigFromJSON() always returns true even if sub-component loads silently fail. Partial boot with broken config becomes invisible.

## Long Term

* check out: https://github.com/trip5/ehdp

* currently we have a chance of collisions if http api + websockets requests happen simulataneously.
  * best to solve this as a mutex (but is this really needed?)

* allow turning off http server (mqtt / serial only)
* allow turning off wifi (serial only)

* add support for revived ESPAsyncWebServer:
  * https://github.com/ESP32Async/ESPAsyncWebServer