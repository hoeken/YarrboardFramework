# ConfigManager v2 Upgrade Guide

This guide covers the breaking changes introduced in ConfigManager v2 and the steps required to update custom controllers built on YarrboardFramework.

## What Changed

ConfigManager v2 restructures the on-device config file from a flat, ad-hoc layout into a versioned schema with three explicit top-level sections: `app`, `guest`, and `admin`. This separation mirrors the permission model (GUEST vs ADMIN) and gives each controller a consistently-scoped namespace.

**Existing v1 config files are migrated automatically on first boot.** No manual intervention is required for devices already in the field.

---

## Config File Format

### v1 (old)

```json
{
  "app": {
    "is_first_boot": false,
    "admin_user": "admin",
    "admin_pass": "admin",
    "guest_user": "guest",
    "guest_pass": "guest",
    "default_role": "nobody"
  },
  "board": {
    "name": "My Board",
    "channels": { ... },
    "rgb": { ... }
  },
  "network": {
    "wifi_ssid": "MySSID",
    "wifi_pass": "MyPass",
    "wifi_mode": "client",
    "local_hostname": "myboard"
  }
}
```

### v2 (new)

```json
{
  "schema_version": 2,
  "guest": {
    "name": "My Board",
    "channels": { ... },
    "rgb": { ... }
  },
  "admin": {
    "is_first_boot": false,
    "auth": {
      "admin_user": "admin",
      "admin_pass": "admin",
      "guest_user": "guest",
      "guest_pass": "guest",
      "default_role": "nobody"
    },
    "network": {
      "wifi_ssid": "MySSID",
      "wifi_pass": "MyPass",
      "wifi_mode": "client",
      "local_hostname": "myboard"
    }
  }
}
```

Key structural changes:
- A top-level `schema_version` field (value `2`) identifies the format.
- The old `board` section becomes `guest`. Public/read-only settings live here.
- The old `network` and `app` sections are merged into `admin`. Sensitive settings live here.
- `is_first_boot` moves from `app` to the root of `admin`.
- Each controller's data lives under its own named key within `guest` or `admin` (e.g., `guest["channels"]`, `admin["auth"]`).

---

## Controller Hook Changes

This is the main breaking change for custom controller code.

### Guest config hooks — `loadConfigHook` / `generateConfigHook`

In v1, `loadConfigHook` received either the entire `board` object or a pre-scoped sub-object depending on whether a matching key existed. The behavior was inconsistent.

In v2, `loadConfigHook` **always** receives the sub-object already scoped to `guest[controller->getName()]`. There is no need to dig into a parent object.

```cpp
// v1 — you might have had to handle both cases
bool MyController::loadConfigHook(JsonVariant config, char* error, size_t len) {
  // config might be root["board"] or root["board"]["mycontroller"]
  _my_value = config["my_value"] | default_value;
  return true;
}

// v2 — config is always guest["mycontroller"]
bool MyController::loadConfigHook(JsonVariant config, char* error, size_t len) {
  _my_value = config["my_value"] | default_value;
  return true;
}
```

`generateConfigHook` works the same way — write directly into `output`, which is already scoped to `guest[controller->getName()]`.

### Admin config hooks — `loadAdminConfigHook` / `generateAdminConfigHook`

In v1, most controllers received the flat `app` object, while `NetworkController` was a special case that received the top-level `network` object.

In v2, all controllers receive `admin[controller->getName()]` — consistently, with no special cases.

```cpp
// v1 — NetworkController was special; everything else got root["app"]
bool MyController::loadAdminConfigHook(JsonVariant config, char* error, size_t len) {
  // config was root["app"] — shared with auth, is_first_boot, etc.
  _admin_value = config["admin_value"] | default_value;
  return true;
}

// v2 — config is always admin["mycontroller"]
bool MyController::loadAdminConfigHook(JsonVariant config, char* error, size_t len) {
  _admin_value = config["admin_value"] | default_value;
  return true;
}
```

### New: Validation hooks

Two optional validation hooks are now available. They are called before loading or saving config and may clamp, trim, or reject values. Returning `false` and writing to `error` cancels the operation.

```cpp
// Called before loadConfigHook (guest)
bool MyController::validateConfigJSON(JsonVariant config, char* error, size_t len) {
  if (config["my_value"].as<int>() > MAX_VALUE) {
    snprintf(error, len, "my_value exceeds maximum of %d", MAX_VALUE);
    return false;
  }
  return true;
}

// Called before loadAdminConfigHook (admin)
bool MyController::validateAdminConfigJSON(JsonVariant config, char* error, size_t len) {
  return true;
}
```

Both have no-op default implementations in `BaseController`, so adding them is optional.

### New: Capabilities hook

A new `generateCapabilitiesHook` is now called when building the `app` section of the config response. Use it to advertise controller features to clients.

```cpp
void MyController::generateCapabilitiesHook(JsonVariant output) {
  output["my_feature_enabled"] = true;
  output["channel_count"] = NUM_CHANNELS;
}
```

This hook has a no-op default implementation. It is only additive — no existing behavior changes.

---

## Migration Checklist for Custom Controllers

1. **Check `loadConfigHook`** — remove any code that digs into a parent `board` object. The `config` parameter is now always scoped to `guest[getName()]`.

2. **Check `loadAdminConfigHook`** — remove any code written to handle the flat `app` object. The `config` parameter is now always scoped to `admin[getName()]`. If you were relying on `app["is_first_boot"]`, read it from `_cfg.isFirstBoot()` instead.

3. **Check `generateConfigHook`** — write directly into `output`. Do not add a controller-named sub-key; the framework adds the outer key automatically.

4. **Check `generateAdminConfigHook`** — same as above; write directly into `output`.

5. **Optionally implement `validateConfigJSON` / `validateAdminConfigJSON`** if your controller has range or format constraints worth enforcing at the framework layer.

6. **Optionally implement `generateCapabilitiesHook`** to advertise controller features in the `app` section.

---

## Automatic v1 Migration

Devices running v1 firmware that upgrade to v2 will have their existing config migrated on first load via `ConfigManager::loadV1Config`. The migration logic:

- Reads `app["is_first_boot"]` and stores it internally.
- Reads `board["name"]` and maps it to `guest["name"]`.
- Dispatches guest hooks from `board[controller_name]` (or the flat `board` object if no named sub-key exists).
- Dispatches admin hooks: `network` controller gets `root["network"]`; all others get `root["app"]`.

The migrated config is written back in v2 format the next time `saveConfig` is called (e.g., after a settings change or reboot).

No code changes are needed to support the migration path — it is handled entirely within `ConfigManager`.
