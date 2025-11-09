/*
  pm_base.cpp - Main source file for PatchMate Base library
  Copyright 2025 Brian Costantino.  All right reserved.

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

#include "pm_base.h"

sec_conf_t secure_config = {0};

bool pm_check_wifi_configured() {
  return !(secure_config.ssid[0] == '\0' || secure_config.password[0] == '\0');
}

bool pm_check_ota_configured() {
  return !(secure_config.pm_host[0] == '\0' ||
           secure_config.pm_client_id[0] == '\0' ||
           secure_config.pm_client_token[0] == '\0');
}



int configure_fs_config() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount file system");
    return 1;
  }

  if (!LittleFS.exists(CONFIG_LFS_PATH)) {
    Serial.println("Config file doesn't exist, creating new one...");
    pm_write_config_to_file();
  }

  if (!pm_load_config()) {
    Serial.println("Failed to load config");
    return 1;
  }

  Serial.println("config loaded");
  return 0;
}

bool pm_write_config_to_file() {
  JsonDocument doc;
  doc["ssid"] = secure_config.ssid;
  doc["password"] = secure_config.password;
  doc["pm_host"] = secure_config.pm_host;
  doc["pm_client_id"] = secure_config.pm_client_id;
  doc["pm_client_token"] = secure_config.pm_client_token;

  File configFile = LittleFS.open(CONFIG_LFS_PATH, "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
    return true;
  }

  return false;
}

bool pm_load_config() {
  File configFile = LittleFS.open(CONFIG_LFS_PATH, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  JsonDocument doc;
  auto error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println("Failed to parse config file");
    configFile.close();
    return false;
  }

  strcpy(secure_config.ssid, doc["ssid"]);
  strcpy(secure_config.password, doc["password"]);
  strcpy(secure_config.pm_host, doc["pm_host"]);
  strcpy(secure_config.pm_client_id, doc["pm_client_id"]);
  strcpy(secure_config.pm_client_token, doc["pm_client_token"]);
  configFile.close();

  return true;
}

pm_error_t pm_deserialize_api_check_response(const char *response, update_check_info_t *_update_info) {
  // TODO: currently uses ArduinoJson, should move this over to a more portable library
  // Serial.print(response);
  JsonDocument json;
  DeserializationError error = deserializeJson(json, response);
  if (error) {
    Serial.printf("failed to deserialize json: %s\n", error.c_str());
    return PM_ERR_CANT_DESERIALIZE;
  }
  
  bool success = strcmp("success", json["status"]) == 0;
  if (!success) {
    // Serial.printf("internal object shows failure despite %d status code\n", status_code);
    Serial.print(response);
    Serial.println();
    return PM_ERR_UNKNOWN;
  }

  // copy data to output struct
  JsonObject data = json["data"];
  bool ua = data["updates_available"].as<bool>();
  String purl = data["releases"][0]["purl"].as<String>();
  _update_info->updates_available = ua;
  strcpy(_update_info->purl, purl.c_str());
  return PM_OK;
}

int pm_tokenize_command(char *command, char **argv) {
  int argc = 0;
  char *token = strtok(command, " ");
  while (token != NULL) {
      argv[argc++] = token;
      token = strtok(NULL, " ");
  }
  return argc;
}

void pm_split_url(const char *url, char **base_url, char **path) {
    // Find the end of the authority (host:port) part
    // This assumes a standard URL format like scheme://host[:port]/path
    const char *protocol_end = strstr(url, "://");
    const char *start_of_path = NULL;

    if (protocol_end) {
        start_of_path = strchr(protocol_end + 3, '/'); // Look for the first '/' after "://"
    } else {
        start_of_path = strchr(url, '/'); // No protocol, assume it starts with a path or host
    }

    if (start_of_path) {
        // Calculate length of base URL
        size_t base_len = start_of_path - url;
        *base_url = (char *)malloc(base_len + 1);
        if (*base_url) {
            strncpy(*base_url, url, base_len);
            (*base_url)[base_len] = '\0';
        }

        // Calculate length of path
        size_t path_len = strlen(start_of_path);
        *path = (char *)malloc(path_len + 1);
        if (*path) {
            strcpy(*path, start_of_path);
        }
    } else {
        // No path found, the entire URL is the base
        *base_url = (char *)malloc(strlen(url) + 1);
        if (*base_url) {
            strcpy(*base_url, url);
        }
        *path = (char *)malloc(1); // Empty string for path
        if (*path) {
            (*path)[0] = '\0';
        }
    }
}

bool str_is_null_or_empty(const char *str) {
  return (str == NULL || *str == '\0');
}

// get patchmate API url to check for updates
int pm_get_check_update_url(char *buffer, const char *host, const char *curr_vers) {
  if (str_is_null_or_empty(curr_vers)) {
    return sprintf(buffer, "http://%s/api/v0/updates", host);
  } else {
    return sprintf(buffer, "http://%s/api/v0/updates?current_versions=%s", host, curr_vers);
  }
}


// void pm_esp_check_for_updates(update_check_info_t *_update_info) {
//   WiFiClient client;
//   HTTPClient http;

//   char check_for_updates_url[255];
//   sprintf(check_for_updates_url, "http://%s/api/v0/ready_to_deploy/?current_versions=%s", secure_config.pm_host, FIRMWARE_PURL);
//   if (!http.begin(client, check_for_updates_url)){
//     Serial.println("[HTTP] Unable to connect");
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   const char *headerKeys[] = {"Content-Type"};
//   http.collectHeaders(headerKeys, 1);
//   http.setAuthorization(secure_config.pm_client_id, secure_config.pm_client_token);
//   Serial.printf("checking for updates from %s\n", check_for_updates_url);
//   int status_code = http.GET();

//   // http status code will be negative on error
//   if (status_code <= 0) {
//     Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(status_code).c_str());
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   Serial.printf("[HTTP] GET %d, content length: %dB, content type: %s\n", status_code, http.getSize(), http.header("Content-Type").c_str());
//   if (status_code >= 400) {
//     Serial.printf("check for updates request failed with status %d\n", status_code);
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   if (strcmp("application/json", http.header("Content-Type").c_str()) != 0) {
//     Serial.println("err: invalid Content-Type, cannot deserialize");
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   String response_content = http.getString();
//   JsonDocument json;
//   DeserializationError error = deserializeJson(json, response_content);
//   if (error) {
//     Serial.printf("failed to deserialize json: %s\n", error.c_str());
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   bool success = strcmp("success", json["status"]) == 0;
//   if (!success) {
//     Serial.printf("internal object shows failure despite %d status code\n", status_code);
//     Serial.print(response_content);
//     Serial.println();
//     _update_info->update_status = UPDATE_CHECK_FAIL;
//     return;
//   }

//   JsonObject data = json["data"];
//   bool updates_available = data["updates_available"].as<bool>();
//   if (!updates_available) {
//     Serial.println("up to date.");
//     _update_info->update_status = UPDATE_CHECK_NO_UPDATES;
//     return;
//   }

//   //Serial.printf("updates available: %d\n", updates_available);
//   //Serial.print(response_content);
//   String event_id = data["event_id"].as<String>();
//   String update_url = data["updates"][0]["bin_url"];
//   String new_purl = data["updates"][0]["purl"].as<String>();
//   http.end();

//   strcpy(_update_info->event_id, event_id.c_str());
//   strcpy(_update_info->update_url, update_url.c_str());
//   strcpy(_update_info->new_purl, new_purl.c_str());
//   _update_info->update_status = UPDATE_CHECK_UPDATES_AVAILABLE;
//   return;
// }

wl_status_t pm_esp_connect_to_wifi_block(unsigned long timeout_ms) {
  unsigned long start = millis();
  WiFi.begin(secure_config.ssid, secure_config.password);
  Serial.print("Connecting");
  wl_status_t wl_status = WiFi.status();
  while (wl_status != WL_CONNECTED) {
    if (millis() >= (start+timeout_ms)) break;
    delay(500);
    Serial.print(".");
    wl_status = WiFi.status();
  }
  Serial.println();

  return wl_status;
}

//see https://gist.github.com/mykeels/bc5eeb7de660bf6e9ac512274f150cc1
void pm_esp_list_wifi_networks() {
  // scan for nearby networks
  int numSsid = WiFi.scanNetworks();
  while (numSsid == -1) {
    Serial.println("Couldn't get a wifi connection");
    delay(2000);
    numSsid = WiFi.scanNetworks();
  }

  // print the list of networks seen
  Serial.printf("%d available networks\n", numSsid);

  // print the network number and name for each network found
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(WiFi.SSID(thisNet));
    // Serial.print("\tSignal: ");
    // Serial.print(WiFi.RSSI(thisNet));
    // Serial.print(" dBm");
    Serial.println();
    //Serial.print("\tEncryption: ");
    //printEncryptionType(WiFi.encryptionType(thisNet));
  }
}

// set a value in sec_conf_t struct
pm_error_t pm_conf_set(const char *key, const char *val) {
  for (size_t i = 0; i < CONFIG_TABLE_SIZE; ++i) {
    if (strcmp(key, config_table[i].key) == 0) {
      void *field = (char *)&secure_config + config_table[i].offset;
      memset(field, 0, config_table[i].size);   // overwrite existing value with zeros
      strncpy((char *)field, val, config_table[i].size - 1);
      return PM_OK;
    }
  }

  return PM_ERR_INVALID_ARG;
}

// get a value from sec_conf_t struct
pm_error_t pm_conf_get(const char *key, char **val) {
  for (size_t i = 0; i < CONFIG_TABLE_SIZE; ++i) {
    if (strcmp(key, config_table[i].key) != 0) continue;

    // set val pointer reference to field value
    void *field = (char *)&secure_config + config_table[i].offset;
    *val = (char *)field;
    return PM_OK;
  }
  return PM_ERR_INVALID_ARG;
}

// used to determine if provided key points to a namespace or specific field
bool pm_conf_is_ns(const char *key) {
  size_t key_len = strlen(key);
  for (size_t i = 0; i < CONFIG_TABLE_SIZE; ++i) {
    const char *entry_key = config_table[i].key;

    // match if entry starts with "key." but isn't exactly "key"
    if (strncmp(entry_key, key, key_len) == 0 && entry_key[key_len] == '.') {
      return true;
    }
  }
  return false;
}

size_t pm_conf_get_ns_fields(const char *ns, const char **out_keys, size_t max_keys) {
  size_t ns_len = strlen(ns);
  size_t count = 0;

  for (size_t i = 0; i < CONFIG_TABLE_SIZE; ++i) {
    const char *key = config_table[i].key;

    // Must start with "namespace."
    if (strncmp(key, ns, ns_len) == 0 && key[ns_len] == '.') {
      const char *suffix = key + ns_len + 1;  // skip "namespace."
      // Only include *direct* children (no further dots)
      if (strchr(suffix, '.') == NULL) {
        if (count < max_keys) {
          out_keys[count] = key;
        }
        count++;
      }
    }
  }

  return count;
}


// int pm_cmd_set(const char *key, const char *val) {
//   if (strcmp("ssid", key) == 0) {
//     strcpy(secure_config.ssid, val);

//   } else if (strcmp("password", key) == 0) {
//     strcpy(secure_config.password, val);

//   } else if (strcmp("pm_host", key) == 0) {
//     strcpy(secure_config.pm_host, val);

//   } else if (strcmp("pm_client_id", key) == 0) {
//     strcpy(secure_config.pm_client_id, val);

//   } else if (strcmp("pm_client_token", key) == 0) {
//     strcpy(secure_config.pm_client_token, val);

//   } else {
//     return 1;
//   }

//   return 0;
// }

// int pm_cmd_get(const char *key, char **val) {

//   if (strcmp("ssid", key) == 0) {
//     *val = secure_config.ssid;

//   } else if (strcmp("password", key) == 0) {
//     *val = secure_config.password;

//   } else if (strcmp("pm_host", key) == 0) {
//     *val = secure_config.pm_host;

//   } else if (strcmp("pm_client_id", key) == 0) {
//     *val = secure_config.pm_client_id;

//   } else if (strcmp("pm_client_token", key) == 0) {
//     *val = secure_config.pm_client_token;

//   } else {
//     return 1;
//   }

//   return 0;
// }


pm_error_t pm_cmd_get_set(int argc, char *argv[]) {
  bool is_set = strcmp("set", argv[0]) == 0;
  if (!is_set && argc < 2) {
    Serial.println("error: get command requires one argument: get <KEY>");
    return PM_ERR_INVALID_ARG;
  }
  if (is_set && argc < 3) {
    Serial.println("error: set command requires two arguments: set <KEY> <VALUE>");
    return PM_ERR_INVALID_ARG;
  }

  pm_error_t status;
  const char *key = argv[1];
  if (is_set) {
    const char *_val = argv[2];
    status = pm_conf_set(key, _val);
    if (status == 0) {
      pm_write_config_to_file();  // update file with changed config
      Serial.println("ok");
    }
  } else {
    // list all namespace
    if (pm_conf_is_ns(key)) {
      const char *keys[CONFIG_TABLE_SIZE];
      size_t count = pm_conf_get_ns_fields(key, keys, CONFIG_TABLE_SIZE);
      for (size_t i = 0; i < count && i < 8; ++i) {
        char *val;
        status = pm_conf_get(keys[i], &val);
        if (status == 0) {
          Serial.printf("%s: %s\n", keys[i], val);
        }
      }

    // print conf key
    } else {
      char *val;
      status = pm_conf_get(key, &val);
      if (status == 0) {
        Serial.printf("%s: %s\n", key, val);
      }
    }
  }

  if (status != 0) {
    Serial.printf("error: key \"%s\" unknown\n", key);
  }

  return status;
}

// void update_started() {
//   Serial.println("CALLBACK:  HTTP update process started");
// }

// void update_finished() {
//   Serial.println("CALLBACK:  HTTP update process finished");
// }

// void update_progress(int cur, int total) {
//   Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
// }

// void update_error(int err) {
//   Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
// }

// void perform_update(char *bin_path) {
//   if ((WiFi.status() != WL_CONNECTED)) {
//     return;
//   }
//   Serial.print(bin_path);
//   char url[256];
//   sprintf(url, "http://%s%s", secure_config.pm_host, bin_path);
//   Serial.print(url);
//   Serial.println();

//   WiFiClient client;

//   // The line below is optional. It can be used to blink the LED on the board during flashing
//   // The LED will be on during download of one buffer of data from the network. The LED will
//   // be off during writing that buffer to flash
//   // On a good connection the LED should flash regularly. On a bad connection the LED will be
//   // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
//   // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
//   ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

//   // Add optional callback notifiers
//   ESPhttpUpdate.onStart(update_started);
//   ESPhttpUpdate.onEnd(update_finished);
//   ESPhttpUpdate.onProgress(update_progress);
//   ESPhttpUpdate.onError(update_error);

//   ESPhttpUpdate.setAuthorization(secure_config.pm_client_id, secure_config.pm_client_token);
//   t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
//   // Or:
//   // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

//   switch (ret) {
//     case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); break;

//     case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); break;

//     case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK"); break;
//   }
// }

// void handle_command() {

//   char command[MAX_COMMAND_LEN];
//   int i = 0;
//   while (Serial.available() > 0) {
//     char incoming_byte = (char)Serial.read();
//     if (i < MAX_COMMAND_LEN)
//       command[i++] = incoming_byte;
//   }
//   Serial.flush();
//   if (command[0] == '\n') {
//     Serial.println();
//     return;
//   }
//   command[strcspn(command, "\n")] = 0;  // remove newline from command

//   // parse command
//   //Serial.print(command);
//   //Serial.println();
//   //printf("Splitting command \"%s\" into tokens:\n", command);
//   char *argv[100];
//   int argc = 0;
//   char *token;
//   token = strtok(command, " ");
//   while (token != NULL) {
//     argv[argc++] = token;
//     token = strtok(NULL, " ");
//   }

//   //Serial.printf("argc: %d\n", argc);
//   //for (int i = 0; i<argc; i++) {
//   //  Serial.printf("%s\n", argv[i]);
//   //}

//   const char *progname = argv[0];
//   if (strcmp("ls", progname) == 0) {
//     Dir dir = LittleFS.openDir("/");
//     int num_files = 0;
//     while (dir.next()) {
//       Serial.print(dir.fileName());
//       if(dir.fileSize()) {
//         File f = dir.openFile("r");
//         Serial.println(f.size());
//       }
//       num_files++;
//     }
//     if (!num_files)
//       Serial.println("no files");
  
//   // handle get/set commands
//   } else if (strcmp("get", progname) == 0 ||
//              strcmp("set", progname) == 0) {
//     cmd_get_set(argc, argv);

//   } else if (strcmp("info", progname) == 0) {
//     print_system_info();

//   } else if (strcmp("wifi", progname) == 0) {
//     if (argc < 2) {
//       if (is_wifi_configured()) {
//         if (WiFi.status() == WL_CONNECTED) {
//           Serial.printf("status: connected to %s\n", WiFi.SSID().c_str());
//         } else {
//           Serial.printf("status: not connected (wl_status: %d)\n", WiFi.status());
//         }
//       } else {
//         Serial.println("status: not configured, set ssid and password then use wifi connect");
//       }
//     } else {
//       if (strcmp("list", argv[1]) == 0) {
//         listNetworks();
//       } else if (strcmp("disconnect", argv[1]) == 0) {
//         WiFi.disconnect();
//       } else if (strcmp("connect", argv[1]) == 0) {
//         connect_to_wifi_block();
//       }
//     }

//   } else if (strcmp("update", progname) == 0) {
//     if (WiFi.status() != WL_CONNECTED) {
//       Serial.println("error: no network connection");
//       return;
//     }

//     if (!is_patchmate_configured()) {
//       Serial.println("error: patchmate is not configured");
//       return;
//     }

//     // check for updates
//     update_check_info_t update_info = {0};
//     check_for_updates(&update_info);
//     if (update_info.update_status == UPDATE_CHECK_UPDATES_AVAILABLE) {
//       Serial.printf("starting update to release %s\n", update_info.new_purl);
//       //Serial.print(update_info.event_id);
//       //Serial.print(update_info.update_url);
//       perform_update(update_info.update_url);
//     }

//   } else {
//     Serial.printf("command \"%s\" not found\n", argv[0]);
//   }
// }

