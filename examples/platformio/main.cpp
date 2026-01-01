/*
  Yarrboard Framework Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <YarrboardFramework.h>

// generated at build by running "gulp" in the firmware directory.
#include "gulp/gulped.h"

YarrboardApp yba;

void setup()
{
  yba.http.registerGulpedFiles(gulpedFiles, gulpedFilesCount);

  yba.board_name = "Framework Test";
  yba.default_hostname = "yarrboard";
  yba.firmware_version = "1.2.3";
  yba.hardware_version = "REV_A_B_C";
  yba.manufacturer = "Test Manufacturer";
  yba.hardware_url = "http://example.com/my-hardware-page";
  yba.project_name = "Yarrboard Framework";
  yba.project_url = "https://github.com/hoeken/YarrboardFramework";

  // register the "test" command that requires GUEST or higher permissions
  yba.protocol.registerCommand(GUEST, "test", [](JsonVariantConst input, JsonVariant output, ProtocolContext context) {
    // check our input.
    String foo = input["foo"] | "Unknown";

    // log to console / webconsole
    YBP.printf("Test Command: %s\n", foo);

    // generate our message back to the client
    output["msg"] = "test";
    output["bar"] = foo;
  });

  yba.setup();
}

void loop()
{
  yba.loop();
}