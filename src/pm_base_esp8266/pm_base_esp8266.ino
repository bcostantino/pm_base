

#include <ESP.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <WiFiClient.h>

#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define FIRMWARE_NAME     "admin/esp8266_sensor_core"
#define FIRMWARE_VERSION  "0.2-dev"
#define FIRMWARE_PURL     FIRMWARE_NAME"@"FIRMWARE_VERSION

typedef struct sec_conf {
  char ssid[32];
  char password[63];

  char pm_host[20];
  char pm_client_id[34];
  char pm_client_token[128];
} sec_conf_t;

typedef struct update_check_info {
  int update_status;
  char event_id[32];
  char update_url[255];
  char new_purl[100];
} update_check_info_t;

#define CONFIG_LFS_PATH "/config.json"
sec_conf_t secure_config = {0};

bool write_config_to_file() {
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

bool loadConfig() {
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

bool is_wifi_configured() {
  return !(secure_config.ssid[0] == '\0' || secure_config.password[0] == '\0');
}

bool is_patchmate_configured() {
  return !(secure_config.pm_host[0] == '\0' ||
           secure_config.pm_client_id[0] == '\0' ||
           secure_config.pm_client_token[0] == '\0');
}

void print_system_info() {
  Serial.println();
  Serial.println("===== ESP8266 System Information =====");

  // Chip Info
  Serial.print("Chip ID: ");
  Serial.println(ESP.getChipId());

  Serial.print("Flash Chip ID: ");
  Serial.println(ESP.getFlashChipId());

  Serial.print("CPU Frequency (MHz): ");
  Serial.println(ESP.getCpuFreqMHz());

  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());

  Serial.print("Core Version: ");
  Serial.println(ESP.getCoreVersion());

  Serial.print("Boot Version: ");
  Serial.println(ESP.getBootVersion());

  Serial.print("Boot Mode: ");
  Serial.println(ESP.getBootMode());

  // Flash Info
  Serial.print("Flash Chip Size: ");
  Serial.println(ESP.getFlashChipSize());

  Serial.print("Flash Real Size: ");
  Serial.println(ESP.getFlashChipRealSize());

  Serial.print("Flash Chip Speed: ");
  Serial.println(ESP.getFlashChipSpeed());

  Serial.print("Flash Mode: ");
  Serial.println(ESP.getFlashChipMode());

  // Free Memory
  Serial.printf("Free Heap Size: %uB\n", ESP.getFreeHeap());
  Serial.printf("Free Sketch Space: %uB\n", ESP.getFreeSketchSpace());

  // Wi-Fi Info
  Serial.println("===== Wi-Fi Information =====");
  Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("Hostname: %s\n", WiFi.hostname().c_str());

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("DNS Server 1: ");
  Serial.println(WiFi.dnsIP(0));

  Serial.print("DNS Server 2: ");
  Serial.println(WiFi.dnsIP(1));

  // Reset Reason
  Serial.println("===== Reset Information =====");
  Serial.print("Reset Reason: ");
  Serial.println(ESP.getResetReason());
  
  Serial.print("Reset Info: ");
  Serial.println(ESP.getResetInfo());
  
  Serial.println("===== End of Information =====");
}


#define UPDATE_CHECK_NO_UPDATES 0
#define UPDATE_CHECK_UPDATES_AVAILABLE 1
#define UPDATE_CHECK_FAIL 2

void check_for_updates(update_check_info_t *_update_info) {
  WiFiClient client;
  HTTPClient http;

  char check_for_updates_url[255];
  sprintf(check_for_updates_url, "http://%s/api/v0/ready_to_deploy/?current_versions=%s", secure_config.pm_host, FIRMWARE_PURL);
  if (!http.begin(client, check_for_updates_url)){
    Serial.println("[HTTP] Unable to connect");
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  const char *headerKeys[] = {"Content-Type"};
  http.collectHeaders(headerKeys, 1);
  http.setAuthorization(secure_config.pm_client_id, secure_config.pm_client_token);
  Serial.printf("checking for updates from %s\n", check_for_updates_url);
  int status_code = http.GET();

  // http status code will be negative on error
  if (status_code <= 0) {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(status_code).c_str());
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  Serial.printf("[HTTP] GET %d, content length: %dB, content type: %s\n", status_code, http.getSize(), http.header("Content-Type").c_str());
  if (status_code >= 400) {
    Serial.printf("check for updates request failed with status %d\n", status_code);
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  if (strcmp("application/json", http.header("Content-Type").c_str()) != 0) {
    Serial.println("err: invalid Content-Type, cannot deserialize");
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  String response_content = http.getString();
  JsonDocument json;
  DeserializationError error = deserializeJson(json, response_content);
  if (error) {
    Serial.printf("failed to deserialize json: %s\n", error.c_str());
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  bool success = strcmp("success", json["status"]) == 0;
  if (!success) {
    Serial.printf("internal object shows failure despite %d status code\n", status_code);
    Serial.print(response_content);
    Serial.println();
    _update_info->update_status = UPDATE_CHECK_FAIL;
    return;
  }

  JsonObject data = json["data"];
  bool updates_available = data["updates_available"].as<bool>();
  if (!updates_available) {
    Serial.println("up to date.");
    _update_info->update_status = UPDATE_CHECK_NO_UPDATES;
    return;
  }

  //Serial.printf("updates available: %d\n", updates_available);
  //Serial.print(response_content);
  String event_id = data["event_id"].as<String>();
  String update_url = data["updates"][0]["bin_url"];
  String new_purl = data["updates"][0]["purl"].as<String>();
  http.end();

  strcpy(_update_info->event_id, event_id.c_str());
  strcpy(_update_info->update_url, update_url.c_str());
  strcpy(_update_info->new_purl, new_purl.c_str());
  _update_info->update_status = UPDATE_CHECK_UPDATES_AVAILABLE;
  return;
}

void connect_to_wifi_block(unsigned long timeout_ms = 10000) {
  unsigned long start = millis();
  WiFi.begin(secure_config.ssid, secure_config.password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() >= (start+timeout_ms)) break;
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) 
    Serial.printf("Successfully connected to WiFi network %s\n", WiFi.SSID().c_str());
  else
    Serial.printf("Failed to connect to WiFi network %s\n", secure_config.ssid);
}

// see https://gist.github.com/mykeels/bc5eeb7de660bf6e9ac512274f150cc1
void listNetworks() {
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
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.println();
    //Serial.print("\tEncryption: ");
    //printEncryptionType(WiFi.encryptionType(thisNet));
  }
}

void setup() {
  Serial.begin(115200); // Initialize serial communication
  delay(2000);          // Wait for Serial to initialize
  Serial.println("Hello, World!");
  Serial.printf("current firmware version: %s\n", FIRMWARE_PURL);

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  if (!LittleFS.exists(CONFIG_LFS_PATH)) {
    Serial.println("Config file doesn't exist, creating new one...");
    write_config_to_file();
  }

  if (!loadConfig()) {
    Serial.println("Failed to load config");
  } else {
    Serial.println("config loaded");
  }

  if (is_wifi_configured()) {
    Serial.printf("auto connecting to WiFi network %s...\n", secure_config.ssid);
    connect_to_wifi_block();

    /*if (WiFi.status() == WL_CONNECTED && is_patchmate_configured()) {
      if ( < UPDATE_CHECK_UPDATES_AVAILABLE) {
        Serial.println("Failed to check for updates! ...");
      }
    }*/

  }
}

int cmd_get_set(int argc, char *argv[]) {
  bool is_set = strcmp("set", argv[0]) == 0;
  if (!is_set && argc < 2) {
    Serial.println("error: get command requires one argument: get <KEY>");
    return 1;
  }
  if (is_set && argc < 3) {
    Serial.println("error: set command requires two arguments: set <KEY> <VALUE>");
    return 1;
  }

  const char *key = argv[1];
  const char *val = argv[2];
  if (strcmp("ssid", key) == 0) {
    if (is_set) {
      strcpy(secure_config.ssid, val);
      write_config_to_file();
    } else {
      Serial.printf("%s: %s\n", key, secure_config.ssid);
    }

  } else if (strcmp("password", key) == 0) {
    if (is_set) {
      strcpy(secure_config.password, val);
      write_config_to_file();
    } else {
      Serial.printf("%s: %s\n", key, secure_config.password);
    }
  } else if (strcmp("pm_host", key) == 0) {
    if (is_set) {
      strcpy(secure_config.pm_host, val);
      write_config_to_file();
    } else {
      Serial.printf("%s: %s\n", key, secure_config.pm_host);
    }
  } else if (strcmp("pm_client_id", key) == 0) {
    if (is_set) {
      strcpy(secure_config.pm_client_id, val);
      write_config_to_file();
    } else {
      Serial.printf("%s: %s\n", key, secure_config.pm_client_id);
    }
  } else if (strcmp("pm_client_token", key) == 0) {
    if (is_set) {
      strcpy(secure_config.pm_client_token, val);
      write_config_to_file();
    } else {
      Serial.printf("%s: %s\n", key, secure_config.pm_client_token);
    }

  } else {
    Serial.printf("error: key \"%s\" unknown\n", key);
    return 1;
  }

  return 0;
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void perform_update(char *bin_path) {
  if ((WiFi.status() != WL_CONNECTED)) {
    return;
  }
  Serial.print(bin_path);
  char url[256];
  sprintf(url, "http://%s%s", secure_config.pm_host, bin_path);
  Serial.print(url);
  Serial.println();

  WiFiClient client;

  // The line below is optional. It can be used to blink the LED on the board during flashing
  // The LED will be on during download of one buffer of data from the network. The LED will
  // be off during writing that buffer to flash
  // On a good connection the LED should flash regularly. On a bad connection the LED will be
  // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
  // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

  // Add optional callback notifiers
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);

  ESPhttpUpdate.setAuthorization(secure_config.pm_client_id, secure_config.pm_client_token);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
  // Or:
  // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

  switch (ret) {
    case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); break;

    case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); break;

    case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK"); break;
  }
}


#define MAX_COMMAND_LEN 256

void handle_command() {

  char command[MAX_COMMAND_LEN];
  int i = 0;
  while (Serial.available() > 0) {
    char incoming_byte = (char)Serial.read();
    if (i < MAX_COMMAND_LEN)
      command[i++] = incoming_byte;
  }
  Serial.flush();
  if (command[0] == '\n') {
    Serial.println();
    return;
  }
  command[strcspn(command, "\n")] = 0;  // remove newline from command

  // parse command
  //Serial.print(command);
  //Serial.println();
  //printf("Splitting command \"%s\" into tokens:\n", command);
  char *argv[100];
  int argc = 0;
  char *token;
  token = strtok(command, " ");
  while (token != NULL) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  //Serial.printf("argc: %d\n", argc);
  //for (int i = 0; i<argc; i++) {
  //  Serial.printf("%s\n", argv[i]);
  //}

  const char *progname = argv[0];
  if (strcmp("ls", progname) == 0) {
    Dir dir = LittleFS.openDir("/");
    int num_files = 0;
    while (dir.next()) {
      Serial.print(dir.fileName());
      if(dir.fileSize()) {
        File f = dir.openFile("r");
        Serial.println(f.size());
      }
      num_files++;
    }
    if (!num_files)
      Serial.println("no files");
  
  // handle get/set commands
  } else if (strcmp("get", progname) == 0 ||
             strcmp("set", progname) == 0) {
    cmd_get_set(argc, argv);

  } else if (strcmp("info", progname) == 0) {
    print_system_info();

  } else if (strcmp("wifi", progname) == 0) {
    if (argc < 2) {
      if (is_wifi_configured()) {
        if (WiFi.status() == WL_CONNECTED) {
          Serial.printf("status: connected to %s\n", WiFi.SSID().c_str());
        } else {
          Serial.printf("status: not connected (wl_status: %d)\n", WiFi.status());
        }
      } else {
        Serial.println("status: not configured, set ssid and password then use wifi connect");
      }
    } else {
      if (strcmp("list", argv[1]) == 0) {
        listNetworks();
      } else if (strcmp("disconnect", argv[1]) == 0) {
        WiFi.disconnect();
      } else if (strcmp("connect", argv[1]) == 0) {
        connect_to_wifi_block();
      }
    }

  } else if (strcmp("update", progname) == 0) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("error: no network connection");
      return;
    }

    if (!is_patchmate_configured()) {
      Serial.println("error: patchmate is not configured");
      return;
    }

    // check for updates
    update_check_info_t update_info = {0};
    check_for_updates(&update_info);
    if (update_info.update_status == UPDATE_CHECK_UPDATES_AVAILABLE) {
      Serial.printf("starting update to release %s\n", update_info.new_purl);
      //Serial.print(update_info.event_id);
      //Serial.print(update_info.update_url);
      perform_update(update_info.update_url);
    }

  } else {
    Serial.printf("command \"%s\" not found\n", argv[0]);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  while (1) {
    if (Serial.available() > 0) {
      handle_command();
    }
    delay(100);
  }
}
