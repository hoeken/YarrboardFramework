# ConfigManager Redesign Plan

## 1. Current Architecture

### JSON Schema (v1)

```json
{
  "network": {
    "local_hostname": "...",
    "wifi_ssid": "...",
    "wifi_pass": "...",
    "wifi_mode": "client|ap",
    "wifi_use_static_ip": false,
    "wifi_static_ip": "...",
    "wifi_gateway": "...",
    "wifi_subnet": "...",
    "wifi_dns1": "...",
    "wifi_dns2": "..."
  },
  "app": {
    "config_version": 1,
    "is_first_boot": false,
    "startup_melody": "STARTUP",
    "app_enable_mfd": false,
    "app_enable_ota": true,
    "app_enable_api": true,
    "app_enable_ssl": false,
    "app_enable_serial": true,
    "app_enable_mqtt": false,
    "app_enable_mqtt_protocol": false,
    "app_enable_ha_integration": false,
    "app_use_hostname_as_mqtt_uuid": false,
    "server_cert": "...",
    "server_key": "...",
    "default_role": "admin|guest|nobody",
    "admin_user": "...",
    "admin_pass": "...",
    "guest_user": "...",
    "guest_pass": "...",
    "ntp_server1": "pool.ntp.org",
    "ntp_server2": "time.nist.gov",
    "gmt_offset_sec": 0,
    "daylight_offset_sec": 0,
    "mqtt_server": "...",
    "mqtt_user": "...",
    "mqtt_pass": "...",
    "mqtt_cert": "..."
  },
  "board": {
    "name": "My Board",
    "uuid": "AA:BB:CC:DD:EE:FF:GG",
    "firmware_version": "1.0.0",
    "hardware_version": "1.0",
    "hardware_url": "...",
    "project_name": "...",
    "project_url": "...",
    "git_url": "...",
    "esp_idf_version": "...",
    "arduino_version": "...",
    "psychic_http_version": "...",
    "yarrboard_framework_version": "...",
    "git_hash": "...",
    "build_time": "...",
    "melodies": ["NONE", "STARTUP", "SUCCESS", ...],
    "capabilities": {
      "buzzer": true,
      "channels": { "count": 4 }
    },
    "channels": [
      { "id": 1, "key": "ch1", ... }
    ]
  }
}
```

### Config Access Levels

| Level | Command | Section |
|---|---|---|
| GUEST | `get_config` | `board` |
| ADMIN | `get_full_config` | `board` + `app` + `network` |
| ADMIN | `get_app_config` | `app` |

### Loading Flow

```
ConfigManager::loadConfigFromFile()
  └── loadConfigFromJSON(root)
        ├── loadNetworkConfigFromJSON(root["network"])  → NetworkController::loadNetworkConfig()
        ├── loadAppConfigFromJSON(root["app"])
        │     ├── NTPController::loadNTPConfig()         ← void, no error propagation
        │     ├── OTAController::loadOTAConfig()         ← void, no error propagation
        │     ├── AuthController::loadAuthConfig()       ← void, no error propagation
        │     ├── HTTPController::loadHTTPConfig()       ← void, no error propagation
        │     ├── ProtocolController::loadSerialConfig() ← void, no error propagation
        │     └── MQTTController::loadMQTTConfig()       ← returns bool, result ignored
        └── loadBoardConfigFromJSON(root["board"])
              └── for each controller: controller->loadConfigHook()  ← returns bool, result ignored
```

### Generation Flow

```
ConfigManager::generateFullConfig(output)
  ├── generateNetworkConfig(output["network"])  → NetworkController::generateNetworkConfig()
  ├── generateAppConfig(output["app"])
  │     ├── NTPController::generateNTPConfig()
  │     ├── OTAController::generateOTAConfig()
  │     ├── AuthController::generateAuthConfig()
  │     ├── HTTPController::generateHTTPConfig()
  │     ├── ProtocolController::generateSerialConfig()
  │     └── MQTTController::generateMQTTConfig()
  └── generateBoardConfig(output["board"])
        ├── static keys (uuid, firmware_version, urls, ...)
        ├── for each controller: controller->generateCapabilitiesHook()
        └── for each controller: controller->generateConfigHook()
```

---

## 2. Problems

### Problem 1: Asymmetric Delegation (MODERATE)

Board config delegates to controllers via virtual hooks (`loadConfigHook` / `generateConfigHook`). App and network config use direct method calls with bespoke signatures (`loadAuthConfig()`, `generateOTAConfig()`, etc.).

This creates two different mental models for the same job. Adding a new admin-level config requires adding a direct call into `loadAppConfigFromJSON()` / `generateAppConfig()`, while adding board-level config just requires implementing the hook.

### Problem 2: Error Propagation Hole (MODERATE)

Both `loadAppConfigFromJSON()` and `loadBoardConfigFromJSON()` always return `true` regardless of whether sub-calls succeed.

In `loadAppConfigFromJSON()`, several sub-calls (`loadNTPConfig`, `loadOTAConfig`, `loadAuthConfig`, `loadHTTPConfig`, `loadSerialConfig`) have `void` return types so failures are silently swallowed. `loadMQTTConfig()` returns `bool` but its result is not checked.

In `loadBoardConfigFromJSON()`, the return value of every `loadConfigHook()` call is ignored — the loop never checks whether any hook reported a failure.

A partial or corrupt `app` or `board` section boots invisibly into a broken state.

`loadNetworkConfigFromJSON()` does propagate errors correctly, but the inconsistency across all three sections makes it easy to introduce future bugs.

### Problem 3: Flat App Config Namespace (MINOR)

All app-level config keys are dumped into a single flat object (`app`). Keys are prefixed (`app_enable_ota`, `app_enable_mqtt`, `ntp_server1`, `mqtt_server`, etc.) to avoid collisions, but the structure provides no grouping. Adding a new subsystem requires careful manual coordination of key names.

### Problem 4: Board Section Mixes Mutable and Immutable Data (MINOR)

The `board` section combines user-configurable state (`name`, controller configs) with read-only runtime metadata (`uuid`, `firmware_version`, hardware URLs, version strings, `melodies`). Clients cannot distinguish which values are writable without out-of-band documentation.

---

## 3. Proposed New Schema (v2)

The redesign splits the top-level config into three sections that match their access/purpose:

| Section | Access | Source | Purpose |
|---|---|---|---|
| `app` | read-only | YarrboardApp + ConfigManager | Versions, URLs, build info, capabilities |
| `guest` | GUEST read, ADMIN write | controllers via hooks | User-configurable board state |
| `admin` | ADMIN only | controllers via hooks | Network, auth, and app settings |

```json
{
  "schema_version": 2,

  "app": {
    "firmware_version": "1.0.0",
    "hardware_version": "1.0",
    "hardware_url": "...",
    "project_name": "...",
    "project_url": "...",
    "git_url": "...",
    "esp_idf_version": "...",
    "arduino_version": "...",
    "psychic_http_version": "...",
    "yarrboard_framework_version": "...",
    "git_hash": "...",
    "build_time": "...",
    "uuid": "AA:BB:CC:DD:EE:FF:GG",
    "capabilities": "..."
  },

  "guest": {
    "name": "My Board",
    "buzzer": {
      "melodies": ["NONE", "STARTUP", "SUCCESS", ...]
    },
    "channels": [
      { "id": 1, "key": "ch1", ... }
    ]
  },

  "admin": {
    "is_first_boot": false,
    "network": {
      "local_hostname": "...",
      "wifi_ssid": "...",
      "wifi_pass": "...",
      "wifi_mode": "client|ap",
      "wifi_use_static_ip": false,
      "wifi_static_ip": "...",
      "wifi_gateway": "...",
      "wifi_subnet": "...",
      "wifi_dns1": "...",
      "wifi_dns2": "..."
    },
    "auth": {
      "default_role": "admin|guest|nobody",
      "admin_user": "...",
      "admin_pass": "...",
      "guest_user": "...",
      "guest_pass": "..."
    },
    "ntp": {
      "ntp_server1": "pool.ntp.org",
      "ntp_server2": "time.nist.gov",
      "gmt_offset_sec": 0,
      "daylight_offset_sec": 0
    },
    "ota": {
      "app_enable_ota": true
    },
    "http": {
      "app_enable_api": true,
      "app_enable_ssl": false,
      "server_cert": "...",
      "server_key": "..."
    },
    "protocol": {
      "app_enable_serial": true,
      "app_enable_mfd": false
    },
    "mqtt": {
      "app_enable_mqtt": false,
      "app_enable_mqtt_protocol": false,
      "app_enable_ha_integration": false,
      "app_use_hostname_as_mqtt_uuid": false,
      "mqtt_server": "...",
      "mqtt_user": "...",
      "mqtt_pass": "...",
      "mqtt_cert": "..."
    },
    "buzzer": {
      "startup_melody": "STARTUP"
    }
  }
}
```

### Notes

- `app` is saved to disk for simplicity, but the values are generated fresh on every `get_config` call.
- `schema_version` is only at the very top level; it does not appear inside sub-objects.
- `admin` absorbs `network` as a named sub-key rather than a peer top-level key.
- `melodies` (read-only list) moves into `guest.buzzer` generated by BuzzerController's `generateConfigHook`. `startup_melody` (writable) moves into `admin.buzzer` via BuzzerController's new admin hook.
- Board metadata (`uuid`, version strings, URLs) moves into `app`, generated by ConfigManager. `uuid` is read-only and hardware-derived, so it belongs in the read-only `app` section. `name` stays in `guest` because it is user-configurable and needed for UI.
- `app_enable_mfd` is currently owned by ConfigManager but will move to ProtocolController under `admin.protocol`.

---

## 4. Target Hook Interface

All controllers that participate in config — whether guest-level or admin-level — adopt the same three-method pattern:

```cpp
// Load config for this controller.
// `config` is already scoped to this controller's sub-key (e.g. admin["ntp"]).
// Returns false and writes a message to error on validation failure.
bool loadConfigHook(JsonVariant config, char* error, size_t len);

// Validate a proposed config before saving.
// `config` is already scoped to this controller's sub-key.
// Trims or clamps invalid values where possible; writes error for hard failures.
// Called before both load and save.
bool validateConfigJSON(JsonVariant config, char* error, size_t err_size);

// Write this controller's current config state into output.
// `output` is already scoped to this controller's sub-key (e.g. guest["buzzer"]).
// The controller writes its keys directly into output — it cannot see or modify
// any other controller's config.
void generateConfigHook(JsonVariant output);
```

ConfigManager is responsible for extracting `controller->getName()` from the section object and passing the pre-scoped sub-object into each hook. A controller never indexes into its own key — it receives its slice directly.

The split between `validate` and `load` is intentional:
- **validate** is the gate — it rejects or repairs incoming JSON before any state is mutated.
- **load** is the commit — it can assume the input is well-formed and just copy values.

### General Flow

```
LOADING:
  validateConfigJSON(incoming)   // trims/rejects invalid values
    └── if ok: loadConfigHook(incoming)

SAVING:
  validateConfigJSON(proposed)   // same gate
    └── if ok: write to file
```

---

## 5. New Loading / Generation Architecture

### New `ConfigManager` Public Interface

```cpp
// Top-level loaders (called during setup)
bool loadConfigFromFile(const char* file, char* error, size_t len);
bool loadConfigFromJSON(JsonVariant config, char* error, size_t len);

// Section loaders (called by loadConfigFromJSON)
bool loadGuestConfigFromJSON(JsonVariant config, char* error, size_t len);
bool loadAdminConfigFromJSON(JsonVariant config, char* error, size_t len);

// Top-level generators (called on get_config, get_guest_config, get_admin_config)
void generateFullConfig(JsonVariant output);
void generateAppConfig(JsonVariant output);     // read-only, not saved
void generateGuestConfig(JsonVariant output);
void generateAdminConfig(JsonVariant output);
```

### New Loading Flow

```
ConfigManager::loadConfigFromJSON(root)
  ├── schema_version check (load v1 / load v2 style as needed)
  ├── loadGuestConfigFromJSON(root["guest"])
  │     └── for each controller: controller->loadConfigHook(root["guest"][controller->getName()])
  └── loadAdminConfigFromJSON(root["admin"])
        └── for each controller: controller->loadAdminConfigHook(root["admin"][controller->getName()])
```

`loadAdminConfigHook` is a new virtual on `BaseController`, alongside the existing `loadConfigHook` (renamed `loadGuestConfigHook` for clarity, or kept as-is since most controllers only participate in one).

### New Generation Flow

```
ConfigManager::generateFullConfig(output)
  ├── output["schema_version"] = 2
  ├── generateAppConfig(output["app"])
  │     ├── static metadata (uuid, firmware, hardware, version strings, URLs) written directly by ConfigManager
  │     └── for each controller: controller->generateCapabilitiesHook(output["app"]["capabilities"])
  ├── generateGuestConfig(output["guest"])
  │     └── for each controller: controller->generateConfigHook(output["guest"][controller->getName()])
  └── generateAdminConfig(output["admin"])
        └── for each controller: controller->generateAdminConfigHook(output["admin"][controller->getName()])
```

---

## 6. Schema Migration

The firmware is the only consumer of the persisted config file. The client always receives freshly generated JSON and never needs to parse old schemas.

Migration strategy: on load, check `root["schema_version"]`. Based on the version, run a v1 or v2 loading function.

```cpp
// Rough shape — exact key mapping follows the schema tables
bool ConfigManager::loadV1JSON(JsonVariant root) {
  // app section is never stored, skip

  for each controller:
    if (root["board"][controller.name])
      loadGuestConfigFromJson(root["board"][controller.name])
    else
      loadGuestConfigFromJson(root["board"]);

  for each controller:
    if (controller == network)
      loadAdminConfigFromJson(root["network"]);
    else
      loadAdminConfigFromJson(root["app"]);

  //handle special or weird fields here.

  return true;
}
```

---

## 7. Per-Controller Changes

| Controller | Guest hook | Admin hook | Notes |
|---|---|---|---|
| ConfigManager | `generateConfigHook` → writes `name` to guest | writes static metadata to `app` section | uuid + version strings move here |
| BuzzerController | `generateConfigHook` unchanged (writes melodies list; no load needed — static data) | new `loadAdminConfigHook` / `generateAdminConfigHook` for `startup_melody` | melodies list moves to guest.buzzer, startup_melody moves to admin.buzzer |
| NetworkController | none | rename `loadNetworkConfig` → `loadAdminConfigHook`; key becomes `admin["network"]` | signatures gain `char* error, size_t len` throughout |
| AuthController | none | rename `loadAuthConfig` → `loadAdminConfigHook`; key becomes `admin["auth"]`; add `validateConfigJSON` | void → bool return; error propagation added |
| NTPController | none | rename `loadNTPConfig` → `loadAdminConfigHook`; key becomes `admin["ntp"]`; add `validateConfigJSON` | void → bool; error propagation added |
| OTAController | none | rename `loadOTAConfig` → `loadAdminConfigHook`; key becomes `admin["ota"]`; add `validateConfigJSON` | void → bool |
| HTTPController | none | rename `loadHTTPConfig` → `loadAdminConfigHook`; key becomes `admin["http"]`; add `validateConfigJSON` | void → bool |
| ProtocolController | none | rename `loadSerialConfig` → `loadAdminConfigHook`; key becomes `admin["protocol"]`; add `validateConfigJSON`; absorb `app_enable_mfd` from ConfigManager | void → bool |
| MQTTController | none | rename `loadMQTTConfig` → `loadAdminConfigHook`; key becomes `admin["mqtt"]` | already returns bool; check result in caller |
| ChannelController (template) | `loadConfigHook` / `generateConfigHook` unchanged | none | no admin config |

---

## 8. BaseController Changes

Add `loadAdminConfigHook` and `generateAdminConfigHook` as new virtual methods with default no-op implementations, alongside the existing guest-level hooks:

```cpp
// Existing (guest-level)
// config/output is pre-scoped to guest[controller->getName()]
virtual bool loadConfigHook(JsonVariant config, char* error, size_t len) { return true; }
virtual void generateConfigHook(JsonVariant output) {}
virtual void generateCapabilitiesHook(JsonVariant config) {}

// New (admin-level)
// config/output is pre-scoped to admin[controller->getName()]
virtual bool loadAdminConfigHook(JsonVariant config, char* error, size_t len) { return true; }
virtual void generateAdminConfigHook(JsonVariant output) {}

// Validation (used by both levels before load and before save)
// config is pre-scoped to the controller's own sub-key
virtual bool validateConfigJSON(JsonVariant config, char* error, size_t len) { return true; }
virtual bool validateAdminConfigJSON(JsonVariant config, char* error, size_t len) { return true; }
```

The `app` section is written entirely by `ConfigManager::generateAppConfig()` directly — there is no hook for it. No controller writes into the `app` section.

---

## 9. Error Propagation Fix

Replace the always-`true` `loadAppConfigFromJSON` and `loadBoardConfigFromJSON` patterns with a proper accumulating error pattern mirroring `loadNetworkConfigFromJSON`:

```cpp
bool ConfigManager::loadAdminConfigFromJSON(JsonVariant config, char* error, size_t len) {
  bool result = true;
  for (const auto& entry : _app.getControllers()) {
    JsonVariant sub = config[entry.controller->getName()];
    if (!entry.controller->loadAdminConfigHook(sub, error, len)) {
      YBP.print(error);
      result = false;
      // continue loading remaining controllers
    }
  }
  return result;
}

bool ConfigManager::loadGuestConfigFromJSON(JsonVariant config, char* error, size_t len) {
  bool result = true;
  for (const auto& entry : _app.getControllers()) {
    JsonVariant sub = config[entry.controller->getName()];
    if (!entry.controller->loadConfigHook(sub, error, len)) {
      YBP.print(error);
      result = false;
    }
  }
  return result;
}
```

ConfigManager extracts each controller's sub-key by name before calling the hook. Each hook receives only its own pre-scoped JSON object and returns false with an error message on failure.

---

## 10. Implementation Order

1. **Add `validateConfigJSON` / `validateAdminConfigJSON` to `BaseController`** — no-op defaults, no callers yet. Zero risk.
2. **Add `loadAdminConfigHook` / `generateAdminConfigHook` to `BaseController`** — no-op defaults. Zero risk.
3. **Implement admin hooks on each controller** — migrate the direct-call implementations one controller at a time, each into `loadAdminConfigHook(sub)` / `generateAdminConfigHook(output)` where `sub` and `output` are the pre-scoped sub-objects. Write `validateAdminConfigJSON` alongside each. Each can be done independently.
4. **Add schema_version handling to `loadConfigFromJSON`** — detect v1, call migration shim, proceed normally.
5. **Replace `loadAppConfigFromJSON` with `loadAdminConfigFromJSON`** — loops over controllers via hooks, accumulates errors.
6. **Replace `generateAppConfig` / `generateNetworkConfig` with `generateAdminConfig`** — loops over hooks, writes sub-objects keyed by `controller->getName()`.
7. **Move board metadata out of `generateBoardConfig`** — static keys (uuid, versions, URLs) move to `generateAppConfig` (written directly by ConfigManager, no hook); `generateConfigHook` output becomes `guest` sub-keys.
8. **Rename `board` → `guest` in the root JSON and update API commands** — `get_config` returns `guest`; `get_full_config` + `get_app_config` are retired in favor of `get_admin_config`.
9. **Move `startup_melody` and `is_first_boot` out of app and into admin** — update BuzzerController's admin hook and ConfigManager accordingly.
10. **Move `melodies` list out of `board`** — it becomes read-only output in `guest.buzzer` from `generateConfigHook`.
11. **Fix guest-level error propagation** — replace the loop in `loadBoardConfigFromJSON` with `loadGuestConfigFromJSON` that extracts the sub-key by name and checks hook return values.
12. **Test schema migration on a device with a v1 config file** — confirm it boots cleanly and saves a v2 config.
