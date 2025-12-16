/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_OTA_H
#define YARR_OTA_H

#include "controllers/BaseController.h"
#include "utility.h"
#include <ArduinoOTA.h>

#define DISABLE_ALL_LIBRARY_WARNINGS
#include <esp32FOTA.hpp>

class YarrboardApp;
class ConfigManager;

class OTAController : BaseController
{
  public:
    OTAController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    void end();
    bool checkOTA();
    void startOTA();

  private:
    esp32FOTA FOTA;

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

    CryptoMemAsset* MyPubKey;
    bool doOTAUpdate = false;
    unsigned long ota_last_message = 0;

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static OTAController* _instance;
    static void _updateBeginFailCallbackStatic(int partition);
    static void _progressCallbackStatic(size_t progress, size_t size);
    static void _updateEndCallbackStatic(int partition);
    static void _updateCheckFailCallbackStatic(int partition, int error_code);

    void _updateBeginFailCallback(int partition);
    void _progressCallback(size_t progress, size_t size);
    void _updateEndCallback(int partition);
    void _updateCheckFailCallback(int partition, int error_code);
};

#endif /* !YARR_OTA_H */