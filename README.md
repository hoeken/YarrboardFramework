# YarrboardFramework

**A comprehensive IoT application framework for ESP32 microcontrollers with modern web-based configuration and control.**

YarrboardFramework provides a complete embedded web server infrastructure for building ESP32-based IoT devices. Designed for reuse across multiple firmware projects, it delivers a standardized, production-ready foundation with rich web interfaces, multi-protocol support, and home automation integration.

## License

**Mozilla Public License Version 2.0 (MPL-2.0)**

All framework source code is licensed under MPL-2.0, allowing commercial use and modification while requiring derivative works to share source code. Example code is released into the Public Domain.

## Features

### Core Capabilities

- **Web-Based Configuration**: Modern single-page application with real-time updates via WebSocket
- **Multi-Protocol Support**: HTTP/HTTPS, WebSocket, MQTT, Serial communication
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
- Support for WebSocket, HTTP API, and Serial interfaces
- Message rate limiting and statistics
- Lambda or member function callbacks

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
  virtual bool setup();                           // Initialization
  virtual void loop();                            // Main execution
  virtual void loadConfigHook(JsonVariantConst);  // Load configuration
  virtual void generateConfigHook(JsonVariant);   // Serialize configuration
  virtual void generateUpdateHook(JsonVariant);   // Real-time data updates
  virtual void generateStatsHook(JsonVariant);    // Statistics generation
  virtual void mqttUpdateHook();                  // MQTT publishing
  virtual void haGenerateDiscoveryHook();         // Home Assistant discovery
  virtual void updateBrightnessHook(uint8_t);     // Global brightness changes
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
- `init()` - Channel initialization with ID
- `loadConfig()` - Configuration loading with validation
- `generateUpdate()` - Real-time status updates
- `mqttUpdate()` - MQTT publishing with hierarchical topics
- `haGenerateDiscovery()` - Home Assistant device registration

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
└── data/
    └── yarrboard.json    # Initial configuration
```

### Basic Integration

```cpp
#include <YarrboardFramework.h>

// Generated by gulp build process
#include "index.html.gz.h"
#include "logo.png.gz.h"

YarrboardApp yba;

void setup() {
  // Set embedded web assets
  yba.http.index_length = index_html_gz_len;
  yba.http.index_sha = index_html_gz_sha;
  yba.http.index_data = index_html_gz;

  yba.http.logo_length = logo_gz_len;
  yba.http.logo_sha = logo_gz_sha;
  yba.http.logo_data = logo_gz;

  // Configure board metadata
  yba.board_name = "My Device";
  yba.default_hostname = "mydevice";
  yba.firmware_version = "1.0.0";
  yba.hardware_version = "REV_A";
  yba.manufacturer = "My Company";
  yba.board_url = "https://github.com/myuser/myproject";
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

  void loadConfigHook(JsonVariantConst config) override {
    // Load configuration from JSON
    setting = config["setting"] | defaultValue;
  }

  void generateConfigHook(JsonVariant config) override {
    // Serialize configuration to JSON
    config["setting"] = setting;
  }

private:
  void handleCommand(JsonVariantConst input, JsonVariant output) {
    // Handle protocol command
    output["result"] = processCommand(input["param"]);
  }

  int setting;
};

// Register in setup()
MyController myController(yba);
yba.registerController(myController);
```

### Custom Channel

```cpp
class MyChannel : public BaseChannel {
public:
  void init(const char* id) override {
    BaseChannel::init(id);
    // Hardware initialization
  }

  void loadConfig(JsonObjectConst config) override {
    BaseChannel::loadConfig(config);
    pin = config["pin"] | -1;
    // Additional configuration
  }

  void generateConfig(JsonObject config) override {
    BaseChannel::generateConfig(config);
    config["pin"] = pin;
  }

  void generateUpdate(JsonObject output) override {
    output["value"] = readValue();
  }

  void mqttUpdate(JsonObject output) override {
    output["state"] = readValue();
  }

  void haGenerateDiscovery() override {
    JsonDocument doc;
    doc["name"] = key;
    doc["state_topic"] = "~/state";
    doc["device_class"] = "sensor";

    _app.mqtt.publishDiscovery("sensor", key, doc.as<JsonObject>());
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
   - Generates C header files with byte arrays
   - Calculates SHA256 for ETag-based caching

2. **Generated Headers**:
   ```cpp
   // Generated by gulp
   const unsigned char index_html_gz[] = { /* compressed data */ };
   const unsigned int index_html_gz_len = 12345;
   const char* index_html_gz_sha = "abc123...";
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

Controllers load configuration via hooks:
```cpp
void MyController::loadConfigHook(JsonVariantConst config) {
  // Access nested configuration
  setting1 = config["setting1"] | defaultValue;

  // Controller-specific section automatically provided
  // Path: config["controllers"]["mycontroller"]
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
void MyChannel::haGenerateDiscovery() {
  JsonDocument doc;
  doc["name"] = key;
  doc["state_topic"] = "~/state";
  doc["command_topic"] = "~/set";
  doc["device_class"] = "switch";

  // Framework handles device info, availability, etc.
  _app.mqtt.publishDiscovery("switch", key, doc.as<JsonObject>());
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
// Logs to Serial, USB CDC, and WebSocket clients
Serial.println("Debug message");

// Add custom print sink
yba.ybp.addPrinter(&myPrintSink);
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
- FastLED by @FastLED
- ETL by @jwellbelove
- esp32FOTA by @chrisjoyce911
- ESP32 Arduino Core by Espressif
