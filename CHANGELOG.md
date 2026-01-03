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
