# v3.0.0

> **Note:** v3.0.0 is a major release with some breaking changes, particularly around the config system and controller architecture. Migration from v2.x should be seamless, but there may be bugs requiring a config edit.  **BACK UP YOUR CONFIG BEFORE YOU UPGRADE**

## ⚠️ Breaking Changes

### Config Field Renames
All `app_enable_*` config keys have been renamed to a consistent `foo_enabled` pattern:

| Old Key | New Key |
|---|---|
| `app_enable_ssl` | `ssl_enabled` |
| `app_enable_api` | `api_enabled` |
| `app_enable_mqtt` | `enabled` (MQTT controller) |
| `app_enable_mqtt_protocol` | `protocol_enabled` |
| `app_enable_ha_integration` | `ha_integration_enabled` |
| `app_use_hostname_as_mqtt_uuid` | `use_hostname_as_uuid` |
| `app_enable_mfd` | `enabled` (Navico controller) |
| `app_enable_ota` | `arduino_ota_enabled` |
| `app_enable_serial` | `serial_enabled` |

### Config Hook Restructuring
- New `sanitizeConfigHook()` and `loadConfigHook()` structure replaces the previous per-controller hooks
- Controllers now use a struct-based config with private `_config` and public `defaults` — all defaults moved out of `YarrboardApp` into their respective controllers
- Various config values have moved from the ConfigManager class into individual Controllers
- Config generation is now `role` and `purpose` aware.  We can now use the same system to generate configs for the UI, for config backup, and shareable configs with private information like passwords removed.

## ✨ New Features

### HTTPS / SSL
- Self-signed TLS certificates can now be generated directly from the Web Server settings page on the ESP32
- HTTP certificate validation added to `HTTPController::validateConfig()`
- Invalid SSL certificate or key on boot now falls back to HTTP instead of locking users out

### Config System
- New unified config saving system for all controllers
- Added `sanitizeConfig()` top-level function, called automatically on every `saveConfig()` as a sanity check
- Added `ConfigPurpose::UI_CONFIG` and `ConfigPurpose::SHAREABLE` — shareable config strips private fields (passwords, keys) before export
- Added `validateConfigHook()` for per-controller config validation
- UUID moved from `ConfigManager` into `NetworkController`
- `generateAppConfig()` moved to `YarrboardApp`
- Added `channels` key to allow extended controller classes more configuration flexibility

### Capabilities
- `generateCapabilitiesHook()` now emits empty arrays for controllers that are registered but not configured — makes it easy to detect whether a controller has been added

### Utilities
- Added `HTTPController::getServer()` accessor for accessing the low level HTTP server

## 🔧 Improvements

### Networking & mDNS
- Removed the mDNS heartbeat (was causing stability issues)
- Multiple reliability improvements to mDNS restart and teardown
- Added guard flag around `mDNS.end()` when restarting to prevent crashes

### Frontend & UI
- Reconnecting to the board now resets all open pages and closes the current one, eliminating the UI flash on reconnect
- Page display now waits for the full config to load before rendering
- All inline `onclick` handlers removed from `index.html` and replaced with jQuery event listeners
- System settings page only fetches the full config when it is opened
- Alerts are now cleared on login and when a new config is loaded

### Security
- Passwords are no longer included in debug log output

## 🐛 Bug Fixes

### C++ / Firmware
- Fixed double-free after failed `malloc` in `ConfigManager::loadConfigFromFile`
- Fixed uninitialized `IntervalTimer._last_us` causing garbage first measurement
- Fixed `sanitizeConfigHook` calling only the local hook instead of all registered hooks
- Fixed `setActive()` not populating the melody table in `BuzzerController`
- Fixed RGB LEDs not appearing in the Improv setup flow
- Fixed minor Navico controller bug
- Fixed missing bracket and channel sanitize logic
- Fixed several `JsonVariant` / `JsonVariantConst` type errors
- Fixed v1 and v2 config parsing edge cases

### JavaScript / Frontend
- Fixed `getStoredTheme()` missing `return` statement (silently broke theme persistence)
- Fixed `setStoredTheme()` missing its `theme` parameter
- Fixed coredump alert using `{msg.git_url}` instead of `${msg.git_url}` in template literal
- Fixed `firmware` and `chdata` loop variables being implicit globals
- Fixed `handleAppConfigMessage` re-registering change handlers on every call — duplicate handlers accumulated across reconnects
- Fixed `msg.hardware_url` access throwing when field was absent
- Fixed `Page.open()` logging `undefined` instead of the page name
- Fixed `setupStatsUI` selector missing leading `#`
- Fixed `addChannel` checking `channelType` before null-guarding the instance
- Fixed `getChannelById` return value not being null-checked before use in update loops
- Fixed wrong `show`/`hide` call for page visibility
- Better alert handling / clearing on login.
- Added documentation links