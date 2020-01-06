#define SETTING_FILE_PATH "/setting.json"
#define PRIMARY_CA_FILE_PATH "/primary_ca.pem"
#define BACKUP_CA_FILE_PATH "/backup_ca.pem"
#define PRIVATE_KEY_FILE_PATH "/private.key"

ESP8266WebServer server(80);

void initializeServer() {
  Serial.print("Starting up httpd...");
  server.begin();
  prepareRouter();
  Serial.println("Done.");

  Serial.println("Starting up SPIFS....");
  SPIFFS.begin();
  Serial.println("Done.");
}

void prepareRouter() {
  server.on("/settings", HTTP_POST, handleSettingPost);
  server.on("/settings", HTTP_GET, handleSettingGet);
  server.on("/rootca.pem", HTTP_GET, handleCAGet);
}

void serverLoop() {
  server.handleClient();
}

boolean writeFile(char* path, String data) {
  Serial.println(data);
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.print(data);
    file.close();
  } else {
    Serial.println("Could not open file" + String(path));
    return false;
  }

  return true;
}

void handleSettingPost() {
  Serial.print("Requested with:");
  Serial.println(server.arg("plain"));

  const int capacity = JSON_OBJECT_SIZE(3) + 2048;
  DynamicJsonDocument assets(capacity);
  DeserializationError error = deserializeJson(assets, server.arg("plain"));
  if (error) {
    Serial.print(F("param deserializeJson() failed: "));
    Serial.println(error.c_str());
    server.send(400);
    return;
  }

  JsonObject setting = assets["settings"];
  if (setting.size() > 0) {
    updateSetting(setting);
  }

  String primary_ca = assets["primary_ca"];
  if (primary_ca.length() > 0) {
    writeFile(PRIMARY_CA_FILE_PATH, primary_ca);
  }

  String backup_ca = assets["backup_ca"];
  if (backup_ca.length() > 0) {
    writeFile(BACKUP_CA_FILE_PATH, backup_ca);
  }

  String private_key = assets["private_key"];
  if (private_key.length() > 0) {
    writeFile(PRIVATE_KEY_FILE_PATH, private_key);
  }

  server.send(200);
}

void updateSetting(JsonObject setting) {
  File file = SPIFFS.open(SETTING_FILE_PATH, "r");
  String savedSettingString;
  if (file) {
    savedSettingString = file.readString();
    file.close();
  } else {
    savedSettingString = "{}";
  }

  Serial.println("saved setting was: " + savedSettingString);

  const size_t CAPACITY = 1024;
  StaticJsonDocument<CAPACITY> savedSettingJSON;

  DeserializationError error = deserializeJson(savedSettingJSON, savedSettingString);
  if (error) {
    Serial.print(F("saved file deserializeJson() failed: "));
    Serial.println(error.c_str());
  }

  if(!setting["project_id"])  setting["project_id"] = savedSettingJSON["project_id"];
  if(!setting["location"])  setting["location"] = savedSettingJSON["location"];
  if(!setting["registry_id"])  setting["registry_id"] = savedSettingJSON["registry_id"];
  if(!setting["device_id"])  setting["device_id"] = savedSettingJSON["device_id"];
  if(!setting["ntp_primary"])  setting["ntp_primary"] = savedSettingJSON["ntp_primary"];
  if(!setting["ntp_secondary"])  setting["ntp_secondary"] = savedSettingJSON["ntp_secondary"];


  file = SPIFFS.open(SETTING_FILE_PATH, "w");
  if (!file) Serial.println("Could not open " + String(SETTING_FILE_PATH));

  if (serializeJson(setting, file) == 0) {
    Serial.println(F("Failed to write to file"));
  } 
  file.close();
}

String readStringFromFile(char path[]) {
  File file = SPIFFS.open(path, "r");
  String result;
  if (file) {
    result = file.readString();
    Serial.print("Readfile: " + String(path) + ":");
    Serial.println(result);
    file.close();
  }

  return result;
}

void handleSettingGet() {
  const int capacity = JSON_OBJECT_SIZE(3) + 1024;
  DynamicJsonDocument assets(capacity);

  String settingJsonString = readStringFromFile(SETTING_FILE_PATH);
  if (settingJsonString.length() > 0) {
    const size_t CAPACITY = JSON_OBJECT_SIZE(6) + 1024;
    StaticJsonDocument<CAPACITY> settingJson;
    DeserializationError error = deserializeJson(settingJson, settingJsonString);
    if (error) {
      Serial.print(F("setting file deserializeJson() failed: "));
      Serial.println(error.c_str());
    }

    assets["settings"] = settingJson;
  }
  String output;
  serializeJson(assets, output);
  server.send(200, "application/json", output);
  assets.clear();
  output.clear();
}

void handleCAGet() {
  String result = readStringFromFile(PRIMARY_CA_FILE_PATH) + readStringFromFile(BACKUP_CA_FILE_PATH);
  server.send(200, "text/plain", result);
}