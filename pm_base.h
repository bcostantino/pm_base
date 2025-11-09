/*
  pm_base.h - Main header file for PatchMate Base library
  Copyright (c) 2025 Brian Costantino.  All right reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef PM_BASE_H
#define PM_BASE_H

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>

#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>

  #include <ESP8266HTTPClient.h>
  #include <ESP8266httpUpdate.h>
  
#endif

#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>


#include <stdbool.h>

#define UPDATE_CHECK_NO_UPDATES 0
#define UPDATE_CHECK_UPDATES_AVAILABLE 1
#define UPDATE_CHECK_FAIL 2

#define MAX_COMMAND_LEN 256

#define CONFIG_LFS_PATH "/config.json"


// secure configuration structure
typedef struct sec_conf {
  char ssid[32];
  char password[63];

  char pm_host[20];
  char pm_client_id[34];
  char pm_client_token[128];
} sec_conf_t;

// patchmate update check structure
typedef struct update_check_info {
  int update_status;
  char event_id[32];
  char update_url[255];
  char new_purl[100];
} update_check_info_t;


bool pm_check_wifi_configured();
bool pm_check_ota_configured();
bool pm_write_config_to_file();
bool pm_load_config();

// utils
int pm_tokenize_command(char *command, char **argv);


// commands
int pm_cmd_set(const char *key, const char *val);
int pm_cmd_get(const char *key, char **val);
int pm_cmd_get_set(int argc, char *argv[]);


// esp specific
void pm_esp_connect_to_wifi_block(unsigned long timeout_ms = 10000);
void pm_esp_list_wifi_networks();

#endif