/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "YarrboardDebug.h"
#include "YarrboardConfig.h"
#include "esp_freertos_hooks.h"
#include <algorithm>

bool has_coredump = false;

YarrboardPrint YBP;
StringPrint startupLogger;

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
    // In ISR context – keep it very light.
    // For pure debugging you *can* do Serial.println, but it's not ideal.
    ets_printf("tick 0\n"); // ISR-safe ROM printf
  }
}

void IRAM_ATTR core1_tick_cb(void)
{
  static uint32_t n = 0;
  if ((n++ & 0xFFF) == 0) {
    // In ISR context – keep it very light.
    // For pure debugging you *can* do Serial.println, but it's not ideal.
    ets_printf("tick 1\n"); // ISR-safe ROM printf
  }
}

void debug_setup()
{
  // startup our serial
  Serial.begin(115200);
  Serial.setTimeout(50);
  YBP.addPrinter(Serial);

  // native usb serial too
  if (ARDUINO_USB_CDC_ON_BOOT) {
    // usb serial takes over the Serial object, but we want to print on both.
    // Serial0 is the regular uart output
    Serial0.begin(115200);
    YBP.addPrinter(Serial0);
  }

  // startup log logs to a string for getting later
  YBP.addPrinter(startupLogger);

  // our debug logs should use YBP too
  esp_log_set_vprintf(debug_log_vprintf);

  if (checkCoreDump()) {
    has_coredump = true;
    YBP.println("WARNING: Coredump Found.");

    saveCoreDumpToFile("/coredump.bin");
  }

  // esp_register_freertos_tick_hook_for_cpu(core0_tick_cb, 0);
  // esp_register_freertos_tick_hook_for_cpu(core1_tick_cb, 1);

  // xTaskCreatePinnedToCore(
  //   heartbeatTask,
  //   "heartbeat",
  //   4096,
  //   NULL,
  //   1,
  //   NULL,
  //   1 // Core 1 (loopTask usually runs on core 1)
  // );
}

String getResetReason()
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

bool checkCoreDump()
{
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK) {
    YBP.print("coredump size: ");
    YBP.println(size);
    const esp_partition_t* pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
    if (pt != NULL)
      return true;
    else
      return false;
  } else
    return false;
}

bool saveCoreDumpToFile(const char* path)
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

    if (esp_partition_read(pt, off, buf, toRead) != ESP_OK)
      return false;

    file.write(buf, toRead);
  }

  file.close();
  return true;
}

bool deleteCoreDump()
{
  if (esp_core_dump_image_erase() == ESP_OK)
    return true;
  else
    return false;
}

void crash_me_hard()
{
  // provoke crash through writing to a nullpointer
  volatile uint32_t* aPtr = (uint32_t*)0x00000000;
  *aPtr = 0x1234567; // goodnight
}

int debug_log_vprintf(const char* fmt, va_list args)
{
  char buf[256];

  // Format the log into a buffer using the va_list
  int len = vsnprintf(buf, sizeof(buf), fmt, args);

  if (len > 0)
    YBP.print(buf);

  return len;
}