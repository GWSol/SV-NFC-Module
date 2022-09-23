/******************************************************************************
  OTA FIRMWARE UPDATE SETUP
******************************************************************************/
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <OneButton.h>

// Include LittleFS header
#include <LittleFS.h>

#define URL_fw_Bin "https://raw.githubusercontent.com/GWSol/SV-NFC-Module/master/NFC-Module/clearHTTP/clearHTTP.bin"
// URL format: "https://raw.githubusercontent.com/(user)/(repo)/(branch)/(path)"

// Get fingerprint of 'https' by visiting this link https://www.grc.com/fingerprints.htm
//#define Fingerprint "70 94 DE DD E6 C4 69 48 3A 92 70 A1 48 56 78 2D 18 64 E0 B7"

OneButton updateButton(D1, true);

/******************************************************************************
  ACCESS POINT SETUP
******************************************************************************/
#include <WiFiClientSecureBearSSL.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
WiFiServer server(80);
String header;

WiFiManager wifiManager;

OneButton resetButton(D1, true);

// Assign location variable for AP config
char output[40] = "";
String Location = "room1";
char send_location[40] = "";

// Flag for saving data
bool shouldSaveConfig = false;

/******************************************************************************
  RFID MODULE SETUP
******************************************************************************/
#include <MFRC522.h>
#define RST_PIN  D3
#define SS_PIN  D0
#define Red D2
#define Blue D4
#define Beep D8
MFRC522 mfrc522(SS_PIN, RST_PIN);
String UID;

String apiKeyValue = "baeb03e1f140d3009647a77cc93f8828";
const char* serverName = "https://persona-hris.com/api/nfc";
String devname;

// Recovery name on FS format
//const String def_devname = "S0421CLA0012";

/******************************************************************************
  CALLBACK NOTIFIERS FOR OTA
******************************************************************************/
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

/******************************************************************************
  SYSTEM SETUP ON BOOT
******************************************************************************/
void setup() {
  pinMode(Red, OUTPUT);
  pinMode(Blue, OUTPUT);
  pinMode(Beep, OUTPUT);

  Serial.begin(115200);

  // Recovery code on FS format
  //WiFi.hostname(def_devname);

  // JSON config readings
  FS_LittleFS();

  // Wifi AP config
  AP_Wifi();

  // Initialize SPI bus
  SPI.begin();

  // Initialize MFRC522
  mfrc522.PCD_Init();

  // Initialize Button
  updateButton.attachLongPressStart(FirmwareUpdate);
  resetButton.attachClick(Reset);
}

/******************************************************************************
  MAIN BODY
******************************************************************************/
void loop() {
  initializeDevID();
  resetButton.tick();
  updateButton.tick();
  if (WiFi.status() == WL_CONNECTED) {
    Enable_blue_LED();

    if (!mfrc522.PICC_IsNewCardPresent()) {
      delay(50);
      return;
    }
    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
      delay(50);
      return;
    }
    readcard();
  }
}

/******************************************************************************
  CALLBACK NOTIFYING US OF THE NEED TO SAVE CONFIG
******************************************************************************/
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/******************************************************************************
  INITIALIZE LittleFS AND AP CONFIG ON BOOT UP
******************************************************************************/
void FS_LittleFS() {
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS...");
    return;
  }
  
  Enable_red_LED();
  // Read config from FS JSON
  Serial.println("Mounting FS...");

  if (LittleFS.exists("/config.json")) {
    // File exists, reading and loading
    Serial.println("Reading config file...");
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      Serial.println("Opened config file...");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      
      json.printTo(Serial);
      
      if (json.success()) {
        Serial.println("\nParsed JSON...");
        strcpy(output, json["output"]);
      }
      else {
        Serial.println("Failed to load JSON config...");
      }
    }
  }
  else
  {
    Serial.println("Failed to mount FS...");
  }
}

void AP_Wifi() {
  WiFiManagerParameter custom_output("output", "Location", output, 40);

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_output);
  wifiManager.autoConnect((const char*)getdevname().c_str());
  strcpy(output, custom_output.getValue());
  strcpy(send_location, custom_output.getValue());
  
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["output"] = output;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing...");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
  Serial.println("\nConnected...");
}

/******************************************************************************
  HELPER ROUTINE TO DUMP A BYTE ARRAY AS HEX VALUES TO Serial
******************************************************************************/
void array_to_string(byte array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++) {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}

/******************************************************************************
  RESET BUTTON
******************************************************************************/
void Reset() {
  Serial.println("Resetting WiFi config...");
  wifiManager.resetSettings();
  delay(500);
  WiFi.disconnect(true);
  delay(500);
  ESP.eraseConfig();
  delay(500);
  ESP.reset();
  delay(500);
  ESP.restart();
}

/******************************************************************************
  SEND LIVE DATA
******************************************************************************/
void Send_live_data(String UIDread, String location, String devid)
{
  // Added code to setup HTTPS client due to moving to HTTPS server
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  // Changed to HTTPClient httpsPost;
  HTTPClient httpsPost;

  // Your Domain name with URL path or IP address with path
  httpsPost.begin(*client, serverName);

  // Specify content-type header
  httpsPost.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Prepare your HTTPS POST request data
  String httpsRequestData = "api_key=" + apiKeyValue + "&cardUID=" + UIDread
                            + "&Location=" + location + "&DevUID=" + devid + "";
  Serial.print("httpsRequestData: ");
  Serial.println(httpsRequestData);

  // Send HTTPS POST request,
  int httpsResponseCode = httpsPost.POST(httpsRequestData);
  // Retrieve response body
  String httpsResponseBody = httpsPost.getString();

  if (httpsResponseCode == 200) {
    // Debug print line, comment when not needed
    //Serial.println("Code is successfully updated. This is a feature.");
    Serial.print("Response Code: ");
    Serial.println(httpsResponseCode);
    Serial.print("Response Body: ");
    Serial.println(httpsResponseBody);
    Serial.println();
    if (httpsResponseBody.substring(11, 15) == "true") {
      if (httpsResponseBody.substring(26, 42) == "\"Access Granted\"" ||
          httpsResponseBody.substring(26, 42) == "\"NFC Registered\"") {
        digitalWrite(Beep, HIGH);
        delay(300);
        digitalWrite(Beep, LOW);
      }
    }
    else if (httpsResponseBody.substring(11, 16) == "false") {
      if (httpsResponseBody.substring(27, 52) == "\"NFC UID not found on DB\"" ||
          httpsResponseBody.substring(27, 43) == "\"Wrong API key.\"") {
        for (int i = 0; i < 3; i++) {
          Enable_red_LED();
          delay(250);
          Disable_LED();
          delay(250);
        }
      }
    }
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpsResponseCode);
  }
  
  // Free resources
  httpsPost.end();
}

/******************************************************************************
  ENABLE RED LED
******************************************************************************/
void Enable_red_LED() {
  digitalWrite(Red, HIGH);
  digitalWrite(Blue, LOW);
}

/******************************************************************************
  ENABLE BLUE LED
******************************************************************************/
void Enable_blue_LED() {
  digitalWrite(Red, LOW);
  digitalWrite(Blue, HIGH);
}

/******************************************************************************
  DISABLE BOTH LEDS
******************************************************************************/
void Disable_LED() {
  digitalWrite(Red, LOW);
  digitalWrite(Blue, LOW);
}

/******************************************************************************
  CARD DETECTION INDICATOR
******************************************************************************/
void Card_detected(int duration) {
  digitalWrite(Red, HIGH);
  digitalWrite(Blue, HIGH);
  delay(duration);
  Disable_LED();
}

/******************************************************************************
  OTA FIRMWARE UPDATE
******************************************************************************/
void FirmwareUpdate() {
  Serial.println("Initializing OTA update...");
  digitalWrite(Blue, LOW);
  ESPhttpUpdate.setLedPin(Red, LOW);
  WiFiClientSecure client;
  client.setInsecure();

  // Added optional callback notifiers
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);

  delay(100);
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, URL_fw_Bin);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s/n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      delay(50);
      OTAerror();
      delay(50);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

/******************************************************************************
  OTA ERROR ACTION
******************************************************************************/
void OTAerror() {
  digitalWrite(Beep, HIGH);
  delay(400);
  digitalWrite(Beep, LOW);
}

/******************************************************************************
  EXTRACT UID FROM CARD AND ADD TO JSON STRUCTURE WITH OTHER DATA
******************************************************************************/
void readcard() {
  Card_detected(200);
  char UIDstr[32] = "";
  // Insert (byte array, length, char array for output)
  array_to_string(mfrc522.uid.uidByte, 4, UIDstr);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  UID = UIDstr;
  Send_live_data(UID, send_location, devname);
}

void initializeDevID() {
  if (devname == NULL) {
    devname = getdevname();
  }
}

String getdevname() {
  LittleFS.begin();

  // Block to check if iotconfig.txt exists
  //  if (!LittleFS.exists("/iotconfig.txt"))
  //  {
  //    Serial.println("iotconfig.txt does not exist!");
  //    Serial.println("Creating new iotconfig.txt...");
  //    //Using "w+" access mode to write to a file and generate if it doesn't exist
  //    File fileToWrite = LittleFS.open("/iotconfig.txt", "w+");
  //    fileToWrite.print(def_devname);
  //    delay(10);
  //    fileToWrite.close();
  //  }

  File fileToRead = LittleFS.open("/iotconfig.txt", "r");
  int i = 0;
  String deviceid;
  while (fileToRead.available()) {
    deviceid = fileToRead.readStringUntil('\n');
    i++;
    Serial.print("Device ID: ");
    Serial.println(deviceid);
  }
  deviceid.trim();
  fileToRead.close();
  String device_id = deviceid;
  return device_id;
}
