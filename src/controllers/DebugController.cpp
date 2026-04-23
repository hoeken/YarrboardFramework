/*
 * Yarrboard Framework
 *
 * Copyright (c) 2025 Zach Hoeken <hoeken@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include "DebugController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

#include <esp_core_dump.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_system.h>

#ifdef YARR_DEBUG_HEARTBEAT
static void heartbeatTask(void* pv)
{
  while (1) {
    Serial.printf("HB %u\n", millis());
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void IRAM_ATTR core0_tick_cb(void)
{
  static uint32_t n = 0;
  if ((n++ & 0xFFF) == 0) {
    ets_printf("tick 0\n");
  }
}

void IRAM_ATTR core1_tick_cb(void)
{
  static uint32_t n = 0;
  if ((n++ & 0xFFF) == 0) {
    ets_printf("tick 1\n");
  }
}
#endif

DebugController::DebugController(YarrboardApp& app) : BaseController(app, "debug"), it(YBP)
{
}

bool DebugController::setup()
{
#ifdef YARR_DEBUG_CRASHME
  registerCommand(ADMIN, "crashme", this, &DebugController::handleCrashMe);
#endif

  // startup our serial
  Serial.begin(115200);
  Serial.setTimeout(50);
  YBP.addPrinter(Serial);

  // native usb serial too
#if ARDUINO_USB_CDC_ON_BOOT
  // usb serial takes over the Serial object, but we want to print on both.
  // Serial0 is the regular uart output
  Serial0.begin(115200);
  YBP.addPrinter(Serial0);
#endif

  // startup log logs to a string for getting later
  YBP.addPrinter(startupLogger);

  // our debug logs should use YBP too
  esp_log_set_vprintf(DebugController::vprintf);

  YBP.println("Yarrboard");
  YBP.print("Hardware Version: ");
  YBP.println(_app.hardware_version);
  YBP.print("Firmware Version: ");
  YBP.println(_app.firmware_version);
  YBP.printf("Firmware build: %s (%s)\n", GIT_HASH, BUILD_TIME);
  YBP.print("Last Reset: ");
  YBP.println(getResetReason());

  // we need littlefs to store our coredump
  if (!LittleFS.begin(true)) {
    YBP.println("ERROR: Unable to mount LittleFS");
  }
  YBP.printf("LittleFS Storage: %d / %d\n", LittleFS.usedBytes(), LittleFS.totalBytes());

  if (checkCoreDump()) {
    has_coredump = true;
    YBP.println("WARNING: Coredump Found.");

    saveCoreDumpToFile("/coredump.bin");
  }

#ifdef YARR_DEBUG_HEARTBEAT
  esp_register_freertos_tick_hook_for_cpu(core0_tick_cb, 0);
  esp_register_freertos_tick_hook_for_cpu(core1_tick_cb, 1);

  xTaskCreatePinnedToCore(
    heartbeatTask,
    "heartbeat",
    4096,
    NULL,
    1,
    NULL,
    1);
#endif

  return true;
}

void DebugController::loop()
{
  // reset our loop times every minute.
  if (INTERVAL(60000))
    it.reset();
}

void DebugController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  if (purpose == ConfigPurpose::UI_CONFIG) {
    output["last_restart_reason"] = _app.debug.getResetReason();
    output["boot_log"] = startupLogger.c_str();

    if (role == ADMIN) {
      if (_app.debug.hasCoredump())
        output["has_coredump"] = _app.debug.hasCoredump();
    }
  }
}

void DebugController::generateStatsHook(JsonVariant output)
{
  if (it.getEntries().empty())
    return;

  JsonArray times = output["loop_timer"].to<JsonArray>();

  for (const auto& e : it.getEntries()) {
    if (e.count == 0)
      continue;

    const uint32_t avg_us = static_cast<uint32_t>(e.total_us / e.count);

    JsonObject entry = times.add<JsonObject>();
    entry["name"] = e.label;
    entry["usec"] = avg_us;
    entry["count"] = e.count;
  }
}

void DebugController::handleCrashMe(JsonVariantConst input, JsonVariant output)
{
  crashMeHard();
}

String DebugController::getResetReason()
{
  switch (esp_reset_reason()) {
    case ESP_RST_UNKNOWN:
      return ("Reset reason can not be determined");
    case ESP_RST_POWERON:
      return ("Reset due to power-on event");
    case ESP_RST_EXT:
      return ("Reset by external pin (not applicable for ESP32)");
    case ESP_RST_SW:
      return ("Software reset via esp_restart");
    case ESP_RST_PANIC:
      return ("Software reset due to exception/panic");
    case ESP_RST_INT_WDT:
      return ("Reset (software or hardware) due to interrupt watchdog");
    case ESP_RST_TASK_WDT:
      return ("Reset due to task watchdog");
    case ESP_RST_WDT:
      return ("Reset due to other watchdogs");
    case ESP_RST_DEEPSLEEP:
      return ("Reset after exiting deep sleep mode");
    case ESP_RST_BROWNOUT:
      return ("Brownout reset (software or hardware)");
    case ESP_RST_SDIO:
      return ("Reset over SDIO");
    default:
      return ("Unknown");
  }
}

bool DebugController::checkCoreDump()
{
  size_t size = 0, address = 0;
  if (esp_core_dump_image_get(&address, &size) != ESP_OK)
    return false;

  YBP.printf("coredump size: %u\n", size);
  const esp_partition_t* pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
  return pt != NULL;
}

bool DebugController::saveCoreDumpToFile(const char* path)
{
  size_t size = 0, address = 0;

  if (esp_core_dump_image_get(&address, &size) != ESP_OK)
    return false;

  const esp_partition_t* pt = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
    "coredump");

  if (!pt)
    return false;

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
    return false;

  uint8_t buf[256];

  for (size_t off = 0; off < size; off += sizeof(buf)) {
    size_t toRead = std::min<size_t>(sizeof(buf), size - off);

    if (esp_partition_read(pt, off, buf, toRead) != ESP_OK) {
      file.close();
      return false;
    }

    file.write(buf, toRead);
  }

  file.close();
  return true;
}

bool DebugController::deleteCoreDump()
{
  if (esp_core_dump_image_erase() == ESP_OK) {
    has_coredump = false;
    return true;
  }
  return false;
}

void DebugController::crashMeHard()
{
  // provoke crash through writing to a nullpointer
  volatile uint32_t* aPtr = (uint32_t*)0x00000000;
  *aPtr = 0x1234567; // goodnight
}

int DebugController::vprintf(const char* fmt, va_list args)
{
  char buf[256];

  // Format the log into a buffer using the va_list
  int len = vsnprintf(buf, sizeof(buf), fmt, args);

  if (len > 0)
    YBP.print(buf);

  if (len >= (int)sizeof(buf))
    YBP.print("[truncated]\n");

  return len;
}