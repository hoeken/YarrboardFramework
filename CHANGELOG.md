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
