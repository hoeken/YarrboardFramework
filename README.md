# YarrboardFramework

**A comprehensive IoT application framework for ESP32 microcontrollers with modern web-based configuration and control.**

YarrboardFramework provides a complete embedded web server infrastructure for building ESP32-based IoT devices. Designed for reuse across multiple firmware projects, it delivers a standardized, production-ready-ish foundation with rich web interfaces, multi-protocol support, and home automation integration.

## License

**Mozilla Public License Version 2.0 (MPL-2.0)**

All framework source code is licensed under MPL-2.0, allowing commercial use and modification while requiring derivative works to share source code. Example code is released into the Public Domain.

## Features

### Core Capabilities

- **Web-Based Configuration**: Modern single-page application with real-time updates via WebSocket
- **JSON-Based Multi-Protocol**: Use the same JSON commands over Websocket, HTTP, Serial, or MQTT
- **Home Assistant Integration**: Automatic MQTT discovery protocol implementation
- **OTA Firmware Updates**: Development (Arduino OTA) and production (signed esp32FOTA) support
- **WiFi Management**: AP/Client modes with Improv provisioning for first-boot setup
- **Role-Based Authentication**: Three-tier access control (NOBODY, GUEST, ADMIN)
- **JSON-Based Configuration**: Human-readable configuration with web editor and backup/restore
- **Performance Monitoring**: IntervalTimer profiling, framerate tracking, and rolling statistics
- **Extensible Architecture**: Modular controller system with lifecycle hooks

### Protocol System

Dynamic JSON-based command protocol with:
- Registration of up to 50 commands (configurable via `YB_PROTOCOL_MAX_COMMANDS`)
- Role-based command access control
- Support for WebSocket, HTTP API, Serial, and MQTT interfaces
- Message rate limiting and statistics
- Lambda or member function callbacks
- Context information (communication mode, user role, client ID) passed to handlers

### Web Interface

- Bootstrap 5 responsive design with dark/light themes
- Real-time data updates via WebSocket
- Pages: Control, Status, Config, Settings, System
- Gzip-compressed assets embedded in firmware
- SHA256 ETag-based caching
- Offline-capable operation

## Architecture

### Controller-Based Design

All subsystems inherit from `BaseController`, providing a unified lifecycle:

```cpp
class BaseController {
  virtual bool setup();                                                                         // Initialization
  virtual void loop();                                                                          // Main execution
  virtual bool loadConfigHook(JsonVariant config, char* error, size_t len);                    // Load configuration with error handling
  virtual void generateConfigHook(JsonVariant config);                                         // Serialize configuration
  virtual void generateCapabilitiesHook(JsonVariant capabilities);                             // Report hardware capabilities
  virtual void generateUpdateHook(JsonVariant output);                                         // Real-time data updates
  virtual bool needsFastUpdate();                                                              // Check if fast update needed
  virtual void generateFastUpdateHook(JsonVariant output);                                     // Fast real-time updates
  virtual void generateStatsHook(JsonVariant output);                                          // Statistics generation
  virtual void mqttUpdateHook(MQTTController* mqtt);                                           // MQTT publishing
  virtual void haUpdateHook(MQTTController* mqtt);                                             // Home Assistant state updates
  virtual void haGenerateDiscoveryHook(JsonVariant components, const char* uuid, MQTTController* mqtt); // Home Assistant discovery
  virtual void updateBrightnessHook(float brightness);                                         // Global brightness changes
};
```

### Built-in Controllers

| Controller | Purpose |
|------------|---------|
| `NetworkController` | WiFi (AP/Client), Improv provisioning, mDNS, DNS server |
| `HTTPController` | Async HTTP/HTTPS server, WebSocket support, client management |
| `ProtocolController` | JSON command protocol with role-based access control |
| `AuthController` | Three-tier role system with session management |
| `MQTTController` | MQTT client with Home Assistant discovery |
| `OTAController` | Arduino OTA (dev) and esp32FOTA (production) with signed firmware |
| `ConfigManager` | JSON configuration storage in LittleFS with validation |
| `DebugController` | Core dumps, logging, IntervalTimer profiling |
| `NTPController` | Network time synchronization |
| `BuzzerController` | PWM piezo buzzer with melody system |
| `RGBController` | FastLED integration with status indication |
| `NavicoController` | UDP multicast publishing for marine electronics |

### Channel System

Template-based hardware abstraction for I/O management:

```cpp
template<typename ChannelType, uint8_t COUNT>
class ChannelController : public BaseController {
  etl::vector<ChannelType, COUNT> channels;
  // Automatic configuration, updates, MQTT, and Home Assistant integration
};
```

Channels inherit from `BaseChannel` and implement:
- `init(uint8_t id)` - Channel initialization with numeric ID (1-N)
- `loadConfig(JsonVariantConst config, char* error, size_t err_size)` - Configuration loading with validation and error reporting
- `generateConfig(JsonVariant config)` - Serialize channel configuration
- `generateUpdate(JsonVariant output)` - Real-time status updates
- `generateStats(JsonVariant output)` - Statistics generation
- `haGenerateDiscovery(JsonVariant doc, const char* uuid, MQTTController* mqtt)` - Home Assistant discovery message
- `haPublishState(MQTTController* mqtt)` - Publish state to Home Assistant
- `haPublishAvailable(MQTTController* mqtt)` - Publish availability to Home Assistant

## Installation

### PlatformIO Registry

Install the library from the PlatformIO Registry by adding it to your `platformio.ini`:

```ini
[env:myenv]
lib_deps =
    hoeken/YarrboardFramework
```

Or install via the PlatformIO CLI:

```bash
pio pkg install --library "hoeken/YarrboardFramework"
```

## Usage

### Project Structure

```
MyProject/
├── platformio.ini        # PlatformIO configuration
├── src/
│   └── main.cpp          # Application entry point
├── html/
│   ├── index.html        # Custom web UI (optional)
│   ├── logo.png          # Project logo
│   ├── js/               # Custom JavaScript
│   └── style.css         # Custom CSS
```

### Basic Integration

```cpp
#include <YarrboardFramework.h>

// Generated by gulp build process
#include "index.html.gz.h"
#include "logo.png.gz.h"

YarrboardApp yba;

// Define GulpedFile structures for web assets
static const GulpedFile index_file = {
  .data = index_html_gz,
  .length = index_html_gz_len,
  .sha256 = index_html_gz_sha,
  .filename = "index.html",
  .mimetype = "text/html"
};

static const GulpedFile logo_file = {
  .data = logo_png_gz,
  .length = logo_png_gz_len,
  .sha256 = logo_png_gz_sha,
  .filename = "logo.png",
  .mimetype = "image/png"
};

void setup() {
  // Set embedded web assets
  yba.http.index = &index_file;
  yba.http.logo = &logo_file;

  // Configure board metadata
  yba.board_name = "My Device";
  yba.default_hostname = "mydevice";
  yba.firmware_version = "1.0.0";
  yba.hardware_version = "REV_A";
  yba.manufacturer = "My Company";
  yba.hardware_url = "https://github.com/myuser/myproject";
  yba.project_url = "https://example.com/myproject";
  yba.project_name = "My Project";

  // Initialize framework
  yba.setup();
}

void loop() {
  yba.loop();
}
```

### Custom Controller

```cpp
class MyController : public BaseController {
public:
  MyController(YarrboardApp& app)
    : BaseController(app, "mycontroller") {}

  bool setup() override {
    // Register protocol commands
    _app.protocol.registerCommand(
      GUEST, "my_command",
      this, &MyController::handleCommand
    );
    return true;
  }

  void loop() override {
    // Controller main loop
  }

  void generateUpdateHook(JsonVariant output) override {
    // Add real-time data to WebSocket updates
    output["my_data"] = getValue();
  }

  bool loadConfigHook(JsonVariant config, char* error, size_t len) override {
    // Load configuration from JSON
    setting = config["setting"] | defaultValue;

    // Validate configuration
    if (setting < 0) {
      snprintf(error, len, "Invalid setting value: %d", setting);
      return false;
    }

    return true;
  }

  void generateConfigHook(JsonVariant config) override {
    // Serialize configuration to JSON
    config["setting"] = setting;
  }

private:
  void handleCommand(JsonVariantConst input, JsonVariant output, ProtocolContext context) {
    // Handle protocol command
    output["result"] = processCommand(input["param"]);

    // Access context information
    // context.mode - communication mode (WEBSOCKET, HTTP, SERIAL, MQTT)
    // context.role - user role (NOBODY, GUEST, ADMIN)
    // context.clientId - unique client identifier
  }

  int setting;
};

// Register in main.cpp setup()
MyController myController(yba);
yba.registerController(myController);
```

### Custom Channel

```cpp
class MyChannel : public BaseChannel {
public:
  void init(uint8_t id) override {
    BaseChannel::init(id);
    // Hardware initialization (channels are numbered 1-N)
  }

  bool loadConfig(JsonVariantConst config, char* error, size_t err_size) override {
    if (!BaseChannel::loadConfig(config, error, err_size))
      return false;

    pin = config["pin"] | -1;
    // Additional configuration

    return true;
  }

  void generateConfig(JsonVariant config) override {
    BaseChannel::generateConfig(config);
    config["pin"] = pin;
  }

  void generateUpdate(JsonVariant output) override {
    output["value"] = readValue();
  }

  void haGenerateDiscovery(JsonVariant doc, const char* uuid, MQTTController* mqtt) override {
    // Populate discovery document
    doc["name"] = name;
    doc["state_topic"] = "~/state";
    doc["device_class"] = "sensor";
    doc["unique_id"] = ha_uuid;

    // Publish to Home Assistant
    mqtt->publishDiscovery("sensor", key, uuid, doc);
  }

  void haPublishState(MQTTController* mqtt) override {
    // Publish current state to Home Assistant
    JsonDocument doc;
    doc["value"] = readValue();
    mqtt->publishHA(key, "state", doc.as<JsonObject>());
  }

private:
  int pin;
  int readValue() { /* ... */ }
};

// Use with ChannelController
using MyChannelController = ChannelController<MyChannel, 8>;
MyChannelController myChannels(yba, "mychannels");
yba.registerController(myChannels);
```

## Build System

### PlatformIO Configuration

```ini
[platformio]
lib_dir = .
src_dir = src
default_envs = default

[env]
platform = espressif32
framework = arduino
board = esp32-s3-devkitc-1
board_build.partitions = default_16MB.csv
board_build.filesystem = littlefs

lib_deps =
    hoeken/YarrboardFramework
    bblanchon/ArduinoJson
    hoeken/PsychicHttp
    # Additional dependencies as needed

extra_scripts =
    pre:scripts/gulp.py          # Build web assets
    pre:scripts/git_version.py   # Embed git version
```

### Web Asset Build Process

The framework uses Gulp to process web assets:

1. **HTML/CSS/JS Processing**:
   - Inlines all CSS and JavaScript
   - Minifies HTML, CSS, and JavaScript
   - Encodes images as base64 data URIs
   - Gzip compresses the final output
   - Generates C header files with GulpedFile structures
   - Calculates SHA256 for ETag-based caching

2. **Generated Headers**:
   ```cpp
   // GulpedFile structure definition
   struct GulpedFile {
     const uint8_t* data;      // Pointer to the file data array
     size_t length;            // Length of the data in bytes
     const char* sha256;       // SHA-256 hash as hex string
     const char* filename;     // Original filename (e.g., "logo.png")
     const char* mimetype;     // MIME type (e.g., "image/png")
   };
   ```

3. **Automatic Build**:
   - PlatformIO `pre:` scripts run Gulp automatically before compilation
   - Git version script embeds commit hash and build timestamp
   - No manual build step required

### Dependencies

Core libraries managed via PlatformIO:
- **ArduinoJson** - JSON parsing and serialization
- **PsychicHttp** - Async HTTP/WebSocket server
- **PsychicMqttClient** - MQTT client
- **FastLED** - RGB LED control
- **ETL (Embedded Template Library)** - Fixed-size containers without dynamic allocation
- **esp32FOTA** - OTA firmware updates with signing
- **improv** - WiFi provisioning protocol

## Configuration

### Configuration Access

Controllers load configuration via hooks with error handling:
```cpp
bool MyController::loadConfigHook(JsonVariant config, char* error, size_t len) {
  // Access nested configuration
  setting1 = config["setting1"] | defaultValue;

  // Validate and report errors
  if (setting1 < 0 || setting1 > 100) {
    snprintf(error, len, "setting1 must be between 0 and 100");
    return false;
  }

  return true;
}
```

## Security

### Authentication System

Three-tier role-based access control:

| Role | Access Level |
|------|-------------|
| `NOBODY` | No access (not authenticated) |
| `GUEST` | Read-only access, safe commands |
| `ADMIN` | Full access, configuration changes, system control |

### Session Management

- Independent sessions for WebSocket, HTTP API, and Serial
- Cookie-based persistent login for HTTP
- Per-connection authentication state
- Configurable credentials via app configuration

### HTTPS Support

Optional SSL/TLS support:
- Custom certificate and private key
- Configurable via PsychicHttp
- Resource-intensive on ESP32 (limit concurrent connections)

## OTA Updates

### Development Mode (Arduino OTA)
- Enabled via PlatformIO upload
- Password-protected
- mDNS-based discovery
- Used during firmware development

### Production Mode (esp32FOTA)
- HTTP/HTTPS firmware downloads
- RSA signature verification with public key
- Firmware manifest URL for version checking
- Progress callbacks for UI updates
- Automatic rollback on boot failure

### Firmware Manifest
```json
{
  "version": "1.2.0",
  "url": "https://example.com/firmware.bin",
  "signature": "base64-encoded-signature"
}
```

## Home Assistant Integration

### Automatic Discovery

Controllers and channels automatically publish MQTT discovery messages:

```cpp
void MyChannel::haGenerateDiscovery(JsonVariant doc, const char* uuid, MQTTController* mqtt) {
  // Populate discovery document
  doc["name"] = name;
  doc["state_topic"] = "~/state";
  doc["command_topic"] = "~/set";
  doc["device_class"] = "switch";
  doc["unique_id"] = ha_uuid;

  // Framework handles device info, availability, etc.
  mqtt->publishDiscovery("switch", key, uuid, doc);
}
```

### MQTT Topics

Hierarchical topic structure:
```
yarrboard/{hostname}/state          # Device state
yarrboard/{hostname}/availability   # Online/offline status
yarrboard/{hostname}/channel1/state # Channel-specific state
```

### Device Information

Automatically included in all discovery messages:
- Device name, manufacturer, model
- Hardware and firmware versions
- Configuration and support URLs
- Unique identifiers

## Performance and Constraints

### System Limits

| Limit | Default | Configurable |
|-------|---------|--------------|
| Maximum controllers | 30 | `YB_MAX_CONTROLLERS` |
| Maximum protocol commands | 50 | `YB_PROTOCOL_MAX_COMMANDS` |
| Maximum HTTP clients | 13 | ESP-IDF limit |
| WebSocket message queue | 100 messages | `HTTPController` |

### Performance Monitoring

Built-in profiling via `IntervalTimer`:
- Per-controller loop execution time
- Rolling average over configurable window
- Accessible via stats API and web interface
- Framerate calculation (main loop Hz)

## Hardware Support

### Primary Target
- **ESP32-S3**: Full support including USB CDC and Bluetooth Improv
- 16MB flash recommended (8MB minimum)
- PSRAM optional but recommended for complex web interfaces

### Peripherals
- **RGB LED**: FastLED-compatible (WS2812, etc.)
- **Buzzer**: PWM-capable piezo buzzer
- **Channels**: GPIO, I2C, SPI, ADC, etc. (application-defined)

## Debugging and Logging

### YarrboardPrint System

Multi-output logging to multiple sinks simultaneously:
```cpp
#include <YarrboardPrint.h>

// Logs to Serial, USB CDC, and WebSocket clients
YBP.println("Debug message");

// Add custom print sink
YBP.addPrinter(&myPrintSink);
```

### Core Dump Support

Automatic detection and extraction of ESP32 core dumps:
- Detects crashes on boot
- Extracts core dump from flash
- Provides base64-encoded dump for analysis
- Clears dump after extraction

### Debug Output

- Startup logs captured and available via WebSocket
- Reset reason detection and reporting
- IntervalTimer profiling per controller
- WebSocket message rate statistics

## Design Philosophy

### Principles

1. **Modularity**: Everything is a controller that can be added or removed
2. **Extensibility**: Easy to add custom controllers and protocol commands
3. **Configuration-Driven**: All settings in JSON, editable via web UI
4. **Real-time First**: WebSocket-based live updates for responsive UIs
5. **Type Safety**: ETL for fixed-size containers without heap allocation
6. **Embedded-Friendly**: Careful memory management, no exceptions
7. **Developer Experience**: Hot-reload, OTA updates, comprehensive logging

### Architectural Patterns

- **Controller Pattern**: All subsystems inherit from `BaseController`
- **Template-Based Channels**: Type-safe hardware abstraction
- **Hook-Based Extension**: Virtual functions for customization
- **JSON Everything**: Configuration, commands, and data transfer

### Constraints

- **Single-Threaded Main Loop**: Must not block
- **No Exceptions**: Embedded-friendly error handling
- **Static Memory**: ETL containers prevent fragmentation
- **ESP32-S3 Focus**: Other ESP32 variants supported but not primary target

## Frontend JavaScript API

The YarrboardFramework provides a comprehensive JavaScript API for building custom web interfaces. The frontend code is organized into modules under the global `YB` namespace.

### Core Components

#### YB.App - Application Core

The main application controller that manages the web interface lifecycle:

```javascript
// Initialize your custom code after the app starts
YB.App.onStart(function() {
  console.log("App is ready!");
});

// Register message handlers for server messages
YB.App.onMessage("custom_message", function(msg) {
  console.log("Received:", msg);
});

// Send commands to the server
YB.client.send({
  cmd: "my_command",
  param1: "value1"
});
```

**Key Methods:**
- `YB.App.onStart(callback)` - Register callback to run when app initializes
- `YB.App.onMessage(messageName, callback)` - Handle messages from the server
- `YB.App.addPage(page)` - Add a custom page to the interface
- `YB.App.removePage(name)` - Remove a page from the interface
- `YB.App.getPage(name)` - Get a page object by name
- `YB.App.openPage(name)` - Navigate to a specific page
- `YB.App.showAlert(message, type)` - Display an alert (types: 'danger', 'success', 'primary')

#### YB.Page - Page Management

Create custom pages with navigation and lifecycle hooks:

```javascript
// Create a custom page
let myPage = new YB.Page({
  name: 'custom',                    // Unique identifier
  displayName: 'My Page',            // Display name in navbar
  permissionLevel: 'guest',          // Access level: 'nobody', 'guest', 'admin'
  showInNavbar: true,                // Show in navigation menu
  position: 'home',                  // Position relative to another page
  ready: true,                       // Page is ready to display immediately
  content: '<h1>Hello World</h1>'    // HTML content
});

// Add lifecycle hooks
myPage.onOpen(function() {
  console.log("Page opened");
  // Start polling, load data, etc.
});

myPage.onClose(function() {
  console.log("Page closed");
  // Stop polling, cleanup, etc.
});

// Add the page to the app
YB.App.addPage(myPage);
```

**Page Properties:**
- `name` - Unique page identifier
- `displayName` - Display name in navigation
- `permissionLevel` - Access control ('nobody', 'guest', 'admin')
- `showInNavbar` - Whether to show in navigation menu
- `position` - Position in navbar ('first', 'last', or another page name)
- `ready` - Whether page is ready to display
- `content` - HTML content of the page

**Page Methods:**
- `setContent(html)` - Update page content
- `onOpen(callback)` - Register callback when page opens
- `onClose(callback)` - Register callback when page closes

#### YB.SettingsPanel - Settings Panel Management

Create custom settings panels that integrate seamlessly with the built-in Settings page:

```javascript
// Create a custom settings panel
let customPanel = new YB.SettingsPanel({
  name: 'custom',                    // Unique identifier
  displayName: 'Custom Settings',    // Display name in settings navbar
  position: 'general',               // Position relative to another panel
  useTitle: true,                    // Show displayName as heading (default: true)
  roundedBorder: true,               // Add border styling (default: true)
  content: '<p>Your settings here</p>' // HTML content
});

// Add lifecycle hooks
customPanel.onOpen(function() {
  console.log("Settings panel opened");
  // Load settings data, start polling, etc.
});

customPanel.onClose(function() {
  console.log("Settings panel closed");
  // Save settings, cleanup, etc.
});

// Add the panel to the settings page
YB.App.addSettingsPanel(customPanel);
```

**SettingsPanel Properties:**
- `name` - Unique panel identifier
- `displayName` - Display name in settings navigation
- `position` - Position in navbar ('first', 'last', or another panel name like 'wifi')
- `useTitle` - Whether to show displayName as a heading (default: true)
- `roundedBorder` - Whether to add border styling (default: true)
- `content` - HTML content of the panel

**SettingsPanel Methods:**
- `setContent(html)` - Update panel content
- `onOpen(callback)` - Register callback when panel opens
- `onClose(callback)` - Register callback when panel closes
- `show()` - Show the panel
- `hide()` - Hide the panel
- `remove()` - Completely remove the panel from the DOM

**App Settings Panel Methods:**
- `YB.App.addSettingsPanel(panel)` - Add a panel to the settings page
- `YB.App.getSettingsPanel(name)` - Get a panel object by name
- `YB.App.removeSettingsPanel(name)` - Remove a panel from the settings page
- `YB.App.openSettingsPanel(name)` - Navigate to and open a specific panel

**Complete Example:**

```javascript
YB.App.onStart(function() {
  // Create a custom settings panel
  let mySettingsPanel = new YB.SettingsPanel({
    name: 'mydevice',
    displayName: 'Device Settings',
    position: 'wifi',  // Insert after WiFi settings
    content: `
      <form id="mySettingsForm">
        <div class="mb-3">
          <label class="form-label">Device Name</label>
          <input type="text" class="form-control" id="deviceName">
        </div>
        <div class="mb-3">
          <label class="form-label">Update Interval (ms)</label>
          <input type="number" class="form-control" id="updateInterval">
        </div>
        <button type="submit" class="btn btn-primary">Save Settings</button>
      </form>
    `
  });

  // Load settings when panel opens
  mySettingsPanel.onOpen(function() {
    // Request current settings from server
    YB.client.send({ cmd: 'get_device_settings' });
  });

  // Handle form submission
  YB.App.onMessage('device_settings', function(msg) {
    $('#deviceName').val(msg.name);
    $('#updateInterval').val(msg.interval);
  });

  $('#mySettingsForm').submit(function(e) {
    e.preventDefault();
    YB.client.send({
      cmd: 'set_device_settings',
      name: $('#deviceName').val(),
      interval: parseInt($('#updateInterval').val())
    });
  });

  YB.App.addSettingsPanel(mySettingsPanel);
});
```

#### YB.ChannelRegistry - Hardware Channel Management

Manage hardware channels (PWM, relays, sensors, etc.) dynamically:

```javascript
// Register a custom channel type
YB.ChannelRegistry.registerChannelType('mychannel', MyChannelClass);

// Get channels by type
let channels = YB.ChannelRegistry.getChannelsByType('pwm');

// Get specific channel by ID
let channel = YB.ChannelRegistry.getChannelById(1, 'pwm');

// Get channel by key
let channel = YB.ChannelRegistry.getChannelByKey('led1', 'pwm');
```

#### YB.BaseChannel - Custom Channel Base Class

Create custom channel types by extending BaseChannel:

```javascript
function MyChannel() {
  YB.BaseChannel.call(this, 'mychannel', 'My Channel');
}
MyChannel.prototype = Object.create(YB.BaseChannel.prototype);

// Define configuration schema for validation
MyChannel.prototype.getConfigSchema = function() {
  return {
    id: { presence: true, numericality: { onlyInteger: true } },
    name: { presence: true, length: { maximum: 63 } },
    enabled: { presence: true },
    pin: { presence: true, numericality: { onlyInteger: true } }
  };
};

// Generate control UI for the home page
MyChannel.prototype.generateControlUI = function() {
  return `
    <div class="card">
      <div class="card-body">
        <h5>${this.name}</h5>
        <button id="btn-${this.id}" class="btn btn-primary">
          Toggle
        </button>
      </div>
    </div>
  `;
};

// Setup UI event handlers
MyChannel.prototype.setupControlUI = function() {
  $(`#btn-${this.id}`).click(() => {
    YB.client.send({
      cmd: 'toggle_channel',
      id: this.id
    });
  });
};

// Update UI with new data
MyChannel.prototype.updateControlUI = function() {
  let state = this.data.state ? 'ON' : 'OFF';
  $(`#btn-${this.id}`).text(state);
};

// Register the channel type
YB.ChannelRegistry.registerChannelType('mychannel', MyChannel);
```

#### YB.Util - Utility Functions

Common utility functions for formatting and UI operations:

```javascript
// Format data
YB.Util.formatBytes(1024 * 1024)           // "1.0 MiB"
YB.Util.formatNumber(1234.567, 2)          // "1,234.57"
YB.Util.secondsToDhms(3665)                // "1 hour, 1 minute"
YB.Util.humanizeText("some_text")          // "Some Text"

// Form validation
YB.Util.showFormValidationResults(data, errors);
YB.Util.flashClass($('#myElement'), 'is-valid', 3000);

// Query parameters
let mode = YB.Util.getQueryVariable('mode');  // Get URL parameter

// Version comparison
if (YB.Util.compareVersions('1.2.0', '1.1.0')) {
  console.log("Newer version available");
}
```

#### YB.log - Logging System

Logging functions for debugging:

```javascript
// Log messages (appear in console and debug terminal)
YB.log("Something happened");
YB.log(`Variable value: ${value}`);

// Setup debug terminal (done automatically)
YB.log.setupDebugTerminal();
```

### WebSocket Client (YB.client)

The YarrboardClient provides WebSocket communication:

```javascript
// Send commands to the server
YB.client.send({
  cmd: "get_status"
});

// Send with priority (bypasses queue)
YB.client.send({ cmd: "urgent" }, true);

// Client connection events are handled automatically
// But you can check connection status:
if (YB.client.isOpen()) {
  console.log("Connected to server");
}

// Get connection status
let status = YB.client.status();  // "CONNECTING", "CONNECTED", "RETRYING", "FAILED"
```

### Standard Message Types

The framework uses these standard message types:

| Message | Description | Trigger |
|---------|-------------|---------|
| `hello` | Initial connection handshake | On WebSocket connect |
| `config` | Board configuration data | On request, config changes |
| `update` | Real-time channel updates | Polled on home page |
| `stats` | System statistics | Polled on stats page |
| `status` | Status messages | Various operations |
| `error` | Error messages | Errors occur |
| `login` | Login response | Authentication |

### Complete Example

Here's a complete example showing how to customize the web interface:

```javascript
// Wait for app to start
YB.App.onStart(function() {

  // Update homepage with custom content
  let homePage = YB.App.getPage('home');
  homePage.setContent(`
    <h1>Welcome to My Device</h1>
    <p>Custom interface for my IoT project.</p>
    <button id="myButton" class="btn btn-primary">Do Something</button>
  `);

  // Setup button handler
  $('#myButton').click(function() {
    YB.client.send({
      cmd: 'custom_command',
      param: 'value'
    });
  });
});

// Create a custom page
let dataPage = new YB.Page({
  name: 'data',
  displayName: 'Data',
  permissionLevel: 'guest',
  showInNavbar: true,
  position: 'home',
  ready: false,
  content: '<div id="dataContent">Loading...</div>'
});

// When page opens, start polling for data
dataPage.onOpen(function() {
  this.pollInterval = setInterval(function() {
    YB.client.send({ cmd: 'get_data' });
  }, 1000);
});

// When page closes, stop polling
dataPage.onClose(function() {
  if (this.pollInterval) {
    clearInterval(this.pollInterval);
  }
});

YB.App.addPage(dataPage);

// Handle custom data messages
YB.App.onMessage('data_response', function(msg) {
  $('#dataContent').html(`
    <h3>Sensor Data</h3>
    <p>Temperature: ${msg.temperature}°C</p>
    <p>Humidity: ${msg.humidity}%</p>
  `);

  // Mark page as ready after first data
  let page = YB.App.getPage('data');
  if (page) page.ready = true;
});

// Handle custom commands from C++ side
YB.App.onMessage('custom_event', function(msg) {
  YB.App.showAlert(`Event: ${msg.description}`, 'info');
});

// Remove pages you don't need
YB.App.removePage('config');
```

### Integration with C++ Backend

The JavaScript communicates with the C++ backend using JSON messages:

**C++ Side:**
```cpp
// Register a command handler
yba.protocol.registerCommand(GUEST, "custom_command",
  [](JsonVariantConst input, JsonVariant output, ProtocolContext context) {
    String param = input["param"];

    // Send response back
    output["msg"] = "custom_event";
    output["description"] = "Command processed";
  }
);

// Broadcast message to all clients
JsonDocument doc;
doc["msg"] = "data_response";
doc["temperature"] = 25.5;
doc["humidity"] = 60.0;
yba.protocol.sendToAll(doc.as<JsonVariant>(), GUEST);
```

**JavaScript Side:**
```javascript
// Send command to C++
YB.client.send({
  cmd: "custom_command",
  param: "value"
});

// Receive response from C++
YB.App.onMessage("custom_event", function(msg) {
  console.log("Event:", msg.description);
});

YB.App.onMessage("data_response", function(msg) {
  console.log("Temperature:", msg.temperature);
  console.log("Humidity:", msg.humidity);
});
```

### Best Practices

1. **Use onStart callbacks** - Always initialize your code inside `YB.App.onStart()` to ensure the DOM is ready
2. **Clean up resources** - Use page `onClose()` callbacks to stop intervals, clear timeouts, etc.
3. **Handle ready states** - Set `page.ready = true` once your page has loaded its data
4. **Validate permissions** - Use appropriate `permissionLevel` for sensitive pages
5. **Use message handlers** - Register handlers with `YB.App.onMessage()` rather than polling
6. **Log for debugging** - Use `YB.log()` which appears in both console and the debug terminal
7. **Check connection** - Verify `YB.client.isOpen()` before sending commands in critical paths

### File Locations

When customizing the web interface, edit these files:

- `html/frontend.js` - Custom JavaScript code
- `html/index.html` - HTML structure (usually provided by framework)
- `html/style.css` - Custom CSS styles
- `html/logo.png` - Your project logo

The build system automatically processes these files and embeds them in the firmware.

## Examples

Complete examples are provided in the `examples/` directory:

- **platformio**: Complete PlatformIO project demonstrating framework integration
- Shows web asset build process, configuration, and custom controller implementation

## Contributing

This project is open source under the Mozilla Public License 2.0. Contributions are welcome:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

Please ensure code follows existing style and includes appropriate documentation.

## Support

- **Issues**: Report bugs and feature requests via GitHub Issues
- **Documentation**: This README and inline code documentation
- **Examples**: See `examples/` directory for complete working implementations

## Acknowledgments

Built on excellent open-source libraries:
- PsychicHttp by @hoeken
- ArduinoJson by @bblanchon
- PsychicMqttClient by @elims
- FastLED by @FastLED
- ETL by @jwellbelove
- esp32FOTA by @chrisjoyce911
- ESP32 Arduino Core by Espressif
- Improv Wifi by [Open Home Foundation](https://www.openhomefoundation.org/)