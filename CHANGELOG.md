# v3.0.0

> **Note:** v3.0.0 is a major release with significant breaking changes, particularly around the config system and controller architecture. Migration from v2.x should be seamless, but there may be bugs requiring a config edit.  **BACK UP YOUR CONFIG BEFORE YOU UPGRADE**

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

# v2.6.2

- Added a heartbeat every 15 minutes to mDNS to hopefully keep that from dying after long uptimes.

# v2.6.1

- Now logging protocol authorization errors to console to avoid spaming get_update errors on reboot.  fixes [#7](https://github.com/hoeken/YarrboardFramework/issues/7)

# v2.6.0

## Features

- Added `page.setBadge()` and `page.clearBadge()` methods to display Bootstrap badge indicators on navbar entries
- Added support for URL-based changelogs in `ota_manifest.json` — changelog field can now point to a remote markdown file instead of inline text
- Changelog is now written to a separate per-board markdown file and linked in `ota_manifest.json`
- Added `ota_complete` message handling for cleaner post-update UI flow

## Improvements

- System page config is now refreshed automatically when the system page is opened

## Bug Fixes

- Fixed OTA progress not completing cleanly when firmware update finishes (fixes #4)
- Fixed `ota_manifest.json` not linking to separate changelog files (fixes #3)

# v2.5.0

## Features

- Added static IP address support for network configuration
- Added `onConfigUpdatedHook()` virtual function to `BaseChannel`, called in `ChannelController::handleConfigCommand` to simplify config change handling in channels
- Added `setWindow()` method to `RollingAverage`

## Improvements

- Separated active and passive buzzer melodies; added morse characters to active buzzer melody list

## Bug Fixes

- `NavicoController` now respects the `app_enable_mfd` configuration option

# v2.4.1

## Bug Fixes

- Fixed stale cookies when password has changed and double submit of login when pressing Enter
- Fixed double instantiation of channels
- Fixed login field corner styling
- Fixed warning with missing `ConfigManager` include
- Fixed multiple callbacks being added in discovery callback
- Fixed alerts being overwritten instead of appended

## Improvements

- Added `MQTTController.getBoardKey()` for getting UUID vs hostname to fix HomeAssistant integration

# v2.3.0

- Added `get(i)` method to `RollingAverage` class for accessing samples by index
- Added build support for all ESP32 chip families (esp32, esp32-c3, esp32-c5, esp32-c6, esp32-s2, esp32-s3)

# v2.2.3

- Forgot to include the gulpfile in its new location.

# v2.2.2

- Consolidated build scripts into single `yarrboard_framework.py` (replaces `git_version.py`, `gulp.py`, `gulpfile.mjs`)
- Added `custom_enable_minify_html` platformio.ini configuration option for HTML minification control
- Improved MQTT error handling and connection management:
  - Added `onDisconnect()` callback hook to detect MQTT disconnection events
  - Added `onError()` callback hook to handle MQTT connection errors
  - Added `_firstConnection` flag to track initial connection attempts
  - First connection errors now properly disconnect and notify admin users with error details
  - Changed `connect()` method to accept optional `waitBlocking` parameter for non-blocking connection attempts
  - Replaced `disconnect()` calls with `forceStop()` for proper MQTT client shutdown

# v2.2.1

- Moved MQTT configuration handling from `ProtocolController` to `MQTTController` for better separation of concerns
- Added MQTT connection status to stats hook via `generateStatsHook()` instead of embedding in protocol controller
- Improved MQTT connection management with proper connect/disconnect lifecycle and connection retry handling
- gulp is no longer minifying JS as it was causing problems.

# v2.2.0

## 🏗️ Controller System

- Added `generateCapabilitiesHook()` to `BaseController` for reporting hardware capabilities
  - Controllers can now populate a `capabilities` object in the board configuration
  - Enables frontend to conditionally show/hide features based on available hardware
  - Implemented in `BuzzerController` (reports `buzzer` and `buzzer_is_active`)
  - Implemented in `RGBController` (reports `rgb` and `rgb_count`)

## 🌐 Frontend & Web UI

- Settings navigation menu layout is now responsive
  - Desktop/tablet (sm+): Left-aligned text, vertical layout
  - Mobile (xs): Centered text, collapsed layout

# v2.1.0

## 🌐 Frontend & Web UI

### Settings & Configuration
- Added JavaScript API for custom settings panels via `api.addSettingsPanel()` and related functions
- Settings page now allows consolidation of settings in a consistent UI
- Added custom settings panel to example firmware

### Firmware Management
- Added ESP Web Tools firmware flasher integration
- Added Improv WiFi support to firmware upload page
- Added firmware manifest generation (`full_manifest.json`) with list of all available firmwares
- Enhanced manifest JSON to include title, board image, and description

## 🔌 Networking & WiFi

- Fixed Improv WiFi implementation (both Bluetooth and WiFi now working)
- Re-factored network controllers to error if WiFi not connected and restart after Improv connects
- Improved WiFi reconnection logic
- When changing WiFi fails, now reconnects to original network

## 🛠️ Developer Experience & Build System

- Updated `make_release.py` to point to `docs/releases` by default
- Refactored git references: renamed `github_url` to `git_url` for consistency across the project
- Git hash and URL links now conditionally displayed/linked based on availability

## 🐛 Bug Fixes

- Fixed unauthorized message issue by skipping queue and sending directly for non-important messages

# v2.0.1

- Fixed a pathing issue with the gulp script on Windows.

# v2.0.0

> **Note:** v2.0.0 represents a major architectural and usability milestone. While not yet declared “production-ready,” this release significantly expands framework capabilities, refines core systems, and stabilizes the frontend, protocol, and OTA infrastructure.

## ✨ Major Features & Enhancements

### Framework & Core Architecture
- HTML/CSS/JS assets are now being compiled from the framework library
- Improved handling of framework directory resolution across build environments
- Increased boot log capacity for better diagnostics
- Fixed duplicate channel loading bugs
- Controller timing interval averages now reset every minute
- Added safeguards and clearer error handling throughout core systems

### Controller & Protocol System
- Added **protocol support over MQTT**
- Unified protocol command handling:
  - `hello`, `login`, and `logout` now use the same `registerHandler()` mechanism
  - Protocol message handlers now receive a **context parameter**
- Consolidated OTA-related protocol commands into `OTAController`
- Fixed OTA message delivery bugs

### OTA & Firmware Updates
- OTA public key moved out of `OTAController.h` and made configurable
- Improved OTA error reporting when firmware is missing
- Updated firmware release and build scripts
- Updated example firmware to use library versioning
- Fixed OTA message delivery issues

## 🌐 Frontend & Web UI

### Page System & Navigation
- Introduced a full **dynamic page API**:
  - `App.addPage()`, `App.getPage()`, `App.removePage()`
  - Page permissions, ordering, navbar visibility, and lifecycle hooks
- Added page lifecycle callbacks:
  - `App.onStart()`, `Page.onOpen()`, `Page.onClose()`
- Converted all built-in pages (`home`, `stats`, `config`, `settings`, `system`, `login`, `logout`) to the new system
- Renamed **Control → Home**
- Added expandable indicators to stats view
- Fixed login flow issues when navigating from logout

### Frontend Architecture
- Refactored and standardized frontend APIs:
  - Unified message handlers (`onMessage`)
  - Standardized update and stats polling (`startUpdatePoller`, `startStatsPoller`, stop methods)
- Expanded example firmware with more frontend and backend code.

### UI / UX Fixes
- Fixed navbar link color state issues
- Fixed navbar layout on vertical mobile views
- Login form element added for autocomplete, etc.
- Startup melody selector hidden when no buzzer is configured

## 🎨 Static Assets, Gulp & Build Pipeline

- Major overhaul of static asset handling:
  - All static files (HTML, CSS, JS, images) are now gulped
  - New unified gulped file format
  - Dynamic logo processing
  - Project-specific CSS and JS loading
  - Clean separation between framework and project CSS
- Improved gulp workflow:
  - Clean temporary build artifacts automatically
  - Removed `.gz` and `_gz` naming from final paths and variables
- Removed deprecated and test files
- Reserved endpoints (`coredump`, `site.manifest`, etc.) are now blocked by default
- Removed OSHW logo

## 📡 Networking, Wi-Fi & Provisioning

- Improved first-boot detection when bundling default `yarrboard.json`
- Enhanced Wi-Fi recovery:
  - Holding boot for 5 seconds re-enables Improv Wi-Fi if connection fails
- Made `IMPROV_BLE` optional
- Moved NimBLE BLE to an external dependency with a compile-time flag

## 🔊 RGB & Buzzer
- RGB controller now supports `maxBrightness`
- Brightness scaling now respects max brightness
- RGB and buzzer support added to examples

## 📚 Documentation & Developer Experience
- Added PlatformIO installation instructions
- Fixed README errors and clarified readiness status
- Updated TODO and roadmap multiple times with future plans
- Improved example configuration and defaults

**v2.0.0 lays the foundation for a stable, extensible, and project-aware Yarrboard ecosystem, with major improvements across protocol handling, frontend architecture, OTA, and build tooling.**

# v1.0.0

Initial release.
