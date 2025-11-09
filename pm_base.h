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


// this is where the sec_conf_t config fields are defined
// to add a new one, add its name along with it's length
// the structure only deals with strings to keep things simple
// consumers are responsible for converting/casting to proper types
#define SECURE_CONFIG_FIELDS \
    X("wifi.ssid",        ssid,             [32]) \
    X("wifi.password",    password,         [64]) \
    X("pm.host",          pm_host,          [64]) \
    X("pm.client_id",     pm_client_id,     [64]) \
    X("pm.client_token",  pm_client_token,  [256]) \
    X("dev.name",         dev_name,         [64]) \





// common c libraries
#include <stdbool.h>

#define ARDUINO_ARCH_ESP32
// imports specific to arduino ESP32
#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>

// imports specific to arduino ESP8266
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>

  #include <ESP8266HTTPClient.h>
  #include <ESP8266httpUpdate.h>

  #include <WiFiClient.h>
  
#endif

#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>


#define MAX_COMMAND_LEN 256

#define CONFIG_LFS_PATH "/config.json"

// general function return codes
typedef enum {
  PM_OK = 0,           // success
  PM_ERR_NULL_PTR,     // null pointer passed
  PM_ERR_OUT_OF_MEMORY,// allocation failure
  PM_ERR_INVALID_ARG,  // invalid argument
  PM_ERR_IO,           // I/O error
  PM_ERR_MISSING_CONFIG,
  PM_ERR_NO_CONNECTION,
  PM_ERR_CONNECTION_REFUSED,
  PM_ERR_BAD_HTTP_STATUS,
  PM_ERR_CANT_DESERIALIZE,
  PM_ERR_UNKNOWN       // catch-all
} pm_error_t;

// secure configuration structure
typedef struct sec_conf {
  // char ssid[32];
  // char password[63];

  // char pm_host[20];
  // char pm_client_id[34];
  // char pm_client_token[128];
#define X(key, name, arr) char name arr;
  SECURE_CONFIG_FIELDS
#undef X
} sec_conf_t;

struct config_map {
  const char *key;
  size_t offset;
  size_t size;
};

static const struct config_map config_table[] = {
#define X(key, name, arr) \
  {key, offsetof(sec_conf_t, name), sizeof(((sec_conf_t *)0)->name)},
  SECURE_CONFIG_FIELDS
#undef X
};

#define CONFIG_TABLE_SIZE (sizeof(config_table) / sizeof(config_table[0]))

// patchmate update check structure
typedef struct update_check_info {
  bool updates_available;
  int response_code;

  char event_id[32];
  
  char purl[256];
  char url[256];

  char error[256];
  
} update_check_info_t;


bool pm_check_wifi_configured();
bool pm_check_ota_configured();


// config
int configure_fs_config();
bool pm_write_config_to_file();
bool pm_load_config();
pm_error_t pm_conf_set(const char *key, const char *val);  // set config values
pm_error_t pm_conf_get(const char *key, char **val);       // get config values

// utils
int pm_tokenize_command(char *command, char **argv);
void pm_split_url(const char *url, char **base_url, char **path);
int pm_get_check_update_url(char *buffer, const char *host, const char *curr_vers);
bool str_is_null_or_empty(const char *str);

// commands
pm_error_t pm_cmd_get_set(int argc, char *argv[]);


// esp specific
wl_status_t pm_esp_connect_to_wifi_block(unsigned long timeout_ms = 10000);
void pm_esp_list_wifi_networks();

// ota
// void pm_esp_check_for_updates(update_check_info_t *_update_info);
pm_error_t pm_deserialize_api_check_response(const char *response, update_check_info_t *_update_info);

#endif