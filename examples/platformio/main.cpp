/*
  Yarrboard Framework Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

#include "controllers/NavicoController.h"
#include <Arduino.h>
#include <YarrboardFramework.h>

// generated at build by running "gulp" in the firmware directory.
#include "src/gulp/gulped.h"

YarrboardApp yba;
NavicoController navico(yba);

void setup()
{
  yba.http.registerGulpedFile(&index_html_gz);
  yba.http.registerGulpedFile(&logo_png_gz);

  yba.board_name = "Framework Test";
  yba.default_hostname = "yarrboard";
  yba.firmware_version = "1.2.3";
  yba.hardware_version = "REV_A_B_C";
  yba.manufacturer = "Test Manufacturer";
  yba.hardware_url = "http://example.com/my-hardware-page";
  yba.project_name = "Yarrboard Framework";
  yba.project_url = "https://github.com/hoeken/YarrboardFramework";

  yba.registerController(navico);
  yba.setup();
}

void loop()
{
  yba.loop();
}