/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "ota.h"
#include "YarrboardDebug.h"

esp32FOTA FOTA(YB_HARDWARE_VERSION, YB_FIRMWARE_VERSION, YB_VALIDATE_FIRMWARE_SIGNATURE);

// my key for future firmware signing
const char* public_key = R"PUBLIC_KEY(
-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAjsPaBVvAoSlNEdxLnKl5
71m+8nEbI6jTenIau884++X+tzjRM/4vctpkfM+b6yPEER6hLKLU5Sr/sVbNAu3s
Ih9UHsgbyzQ4r+NMzM8ohvPov1j5+NgzoIRPn9IQR40p/Mr3T31MXoeSh/WXw7yJ
BjVH2KhTD14e8Yc9CiEUvzYhFVjs8Doy1q2+jffiutcR8z+zGBSGHI3klTK8mNau
r9weglTUCObkUfbgrUWXOkDN50Q97OOv99+p8NPkcThZYbaqjbrOCO9vnMFB9Mxj
5yDruS9QF/qhJ5mC7AuHLhAGdkPu+3OXRDlIJN1j7y8SorvQj9F17B8wnhNBfDPN
QbJc4isLIIBGECfmamCONi5tt6fcZC/xZTxCiEURG+JVgUKjw+mIBrv+iVn9NKYK
UF8shPfl0CGKzOvsXBf91pqF5rHs6TpVw985u1VFbRrUL6nmsCELFxBz/+y83uhj
jsROITwP34vi7qMuHm8UzTnfxH0dSuI6PfWESIM8aq6bidBgUWlnoN/zQ/pwLVsz
0Gh5tAoFCyJ+FZiKS+2spkJ5mJBMY0Ti3dHinp6E2YNxY7IMV/4E9oK+MzvX1m5s
rgu4zp1Wfh2Q5QMX6bTrDCTn52KdyJ6z2WTnafaA08zeKOP+uVAPT0HLShF/ITEX
+Cd7GvvuZMs80QvqoXi+k8UCAwEAAQ==
-----END PUBLIC KEY-----
)PUBLIC_KEY";

CryptoMemAsset* MyPubKey = new CryptoMemAsset("RSA Key", public_key, strlen(public_key) + 1);

bool doOTAUpdate = false;
unsigned long ota_last_message = 0;

void ota_setup()
{
  if (app_enable_ota) {
    ArduinoOTA.setHostname(config.local_hostname);
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setPassword(admin_pass);
    ArduinoOTA.begin();
  }

  FOTA.setManifestURL("https://raw.githubusercontent.com/hoeken/yarrboard-firmware/main/firmware.json");
  FOTA.setPubKey(MyPubKey);
  FOTA.useBundledCerts();

  FOTA.setUpdateBeginFailCb([](int partition) {
    YBP.printf("[ota] Update could not begin with %s partition\n", partition == U_SPIFFS ? "spiffs" : "firmware");
  });

  // usage with lambda function:
  FOTA.setProgressCb([](size_t progress, size_t size) {
    if (progress == size || progress == 0)
      YBP.println();
    YBP.print(".");

    // let the clients know every second and at the end
    if (millis() - ota_last_message > 1000 || progress == size) {
      float percent = (float)progress / (float)size * 100.0;
      sendOTAProgressUpdate(percent);
      ota_last_message = millis();
    }
  });

  FOTA.setUpdateEndCb([](int partition) {
    YBP.printf("[ota] Update ended with %s partition\n", partition == U_SPIFFS ? "spiffs" : "firmware");
    sendOTAProgressFinished();
  });

  FOTA.setUpdateCheckFailCb([](int partition, int error_code) {
    YBP.printf("[ota] Update could not validate %s partition (error %d)\n", partition == U_SPIFFS ? "spiffs" : "firmware", error_code);
    // error codes:
    //  -1 : partition not found
    //  -2 : validation (signature check) failed
  });

  FOTA.printConfig();
}

void ota_loop()
{
  if (doOTAUpdate) {
    FOTA.handle();
    doOTAUpdate = false;
  }

  if (app_enable_ota) {
    ArduinoOTA.handle();
  }
}

void ota_end()
{
  if (!app_enable_ota)
    ArduinoOTA.end();
}