/******************************************************************************
  OTA Fimware Update
 *****************************************************************************/
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <OneButton.h>
#define URL_fw_Bin "https://raw.githubusercontent.com/GWSol/SV-NFC-Module/master/NFC-Module/clearHTTP/clearHTTP.bin"
//URL format: https://raw.githubusercontent.com/${user}/${repo}/${branch}/${path}
//get fingerprint of 'https' by visiting this link https://www.grc.com/fingerprints.htm
//#define Fingerprint "70 94 DE DD E6 C4 69 48 3A 92 70 A1 48 56 78 2D 18 64 E0 B7"
OneButton updateButton(D1, true);
/******************************************************************************
  Access point
 *****************************************************************************/
#include <WiFiClientSecureBearSSL.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
WiFiServer server(80);
String header;

WiFiManager wifiManager;

OneButton resetButton(D1, true);

//assign location variable for AP config
char output[40] = "";
String Location = "room1";
char send_location[40] = "";

//flag for saving data
bool shouldSaveConfig = false;

/******************************************************************************
  RFID MODULE
 *****************************************************************************/
#include <MFRC522.h>
#define RST_PIN  D3
#define SS_PIN  D0
#define Red D2
#define Blue D4
#define Beep D8
MFRC522 mfrc522(SS_PIN, RST_PIN);
String UID;

//String apiKeyValue = "tPmAT5Ab3j7F9";
String apiKeyValue = "baeb03e1f140d3009647a77cc93f8828";
//const char* serverName = "http://dummyendpoint.000webhostapp.com/post-esp-data.php";
const char* serverName = "https://persona-hris.com/api/nfc";
String devname;

/******************************************************************************
  CALLBACK NOTIFIER FOR OTA
 *****************************************************************************/
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

void setup()
{
  pinMode(Red, OUTPUT);
  pinMode(Blue, OUTPUT);
  pinMode(Beep, OUTPUT);

  Serial.begin(115200);

  //initialize SPIFFS
  Initialize_SPIFFS();

  //Json config readings
  FS_Spiffs();

  //Wifi AP config
  AP_Wifi();

  // Init SPI bus
  SPI.begin();

  // Init MFRC522
  mfrc522.PCD_Init();

  // Initialize Button
  updateButton.attachLongPressStart(FirmwareUpdate);
  resetButton.attachClick(Reset);
}

void loop()
{
  initializeDevID();
  resetButton.tick();
  updateButton.tick();
  if (WiFi.status() == WL_CONNECTED)
  {
    Enable_blue_LED();

    if ( ! mfrc522.PICC_IsNewCardPresent())
    {
      delay(50);
      return;
    }
    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
    {
      delay(50);
      return;
    }
    readcard();
  }
}

/******************************************************************************
  Callback notifying us of the need to save config
 *****************************************************************************/
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
/******************************************************************************
  For AP config during boot up
 *****************************************************************************/
void FS_Spiffs()
{
  Enable_red_LED();
  //read config from FS json
  //read configuration from FS json
  Serial.println("mounting FS...");

  //if (SPIFFS.begin())
  //{
  //Serial.println("mounted file system");
  if (SPIFFS.exists("/config.json"))
  {
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile)
    {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success())
      {
        Serial.println("\nparsed json");
        strcpy(output, json["output"]);
        //strcpy(orgID, json["orgID"]); //im pretty sure this is what cause the crash.
      }
      else
      {
        Serial.println("failed to load json config");
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
}

void AP_Wifi()
{
  WiFiManagerParameter custom_output("output", "Location", output, 40);
  //WiFiManagerParameter org_UID("orgID", "Organization ID", orgID, 40, "type=\"number\" min=\"0\"");

  //for test only. disable when satisfied
  //wifiManager.resetSettings();

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_output);
  //wifiManager.addParameter(&org_UID);
  wifiManager.autoConnect((const char*)getdevname().c_str());
  strcpy(output, custom_output.getValue());
  strcpy(send_location, custom_output.getValue());
  //strcpy(orgID, org_UID.getValue());
  //strcpy(send_orgID, org_UID.getValue());
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["output"] = output;
    //json["orgID"] = orgID;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
  Serial.println("/n Connected.");
}
/******************************************************************************
  Helper routine to dump a byte array as hex values to Serial
 *****************************************************************************/
void array_to_string(byte array[], unsigned int len, char buffer[])
{
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}
/******************************************************************************
  Initialize SPIFFS Library
 *****************************************************************************/
void Initialize_SPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}
/******************************************************************************
  Reset Button
*****************************************************************************/
void Reset()
{
  Serial.println("Resetting WiFi config....");
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
  Send live data
*****************************************************************************/
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

  // Send HTTPS POST request
  int httpsResponseCode = httpsPost.POST(httpsRequestData);

  String httpsResponseBody = httpsPost.getString();

  // if (httpsResponseCode = 200), changed operator from
  // '=' to '==', and replaced '200' with 'HTTP_CODE_OK'
  if (httpsResponseCode == 200) {
    Serial.print("Response Code: ");
    Serial.println(httpsResponseCode);
    Serial.print("Response Body: ");
    Serial.println(httpsResponseBody);
    Serial.println();
    if (httpsResponseBody.substring(11, 15) == "true")
    {
      if (httpsResponseBody.substring(26, 42) == "\"Access Granted\"" ||
          httpsResponseBody.substring(26, 42) == "\"NFC Registered\"")
      {
        Serial.println("Code is successfully updated. This is a new feature.");
        digitalWrite(Beep, HIGH);
        delay(300);
        digitalWrite(Beep, LOW);
      }
    }
    else if (httpsResponseBody.substring(11, 16) == "false")
    {
      if (httpsResponseBody.substring(27, 52) == "\"NFC UID not found on DB\"" ||
          httpsResponseBody.substring(27, 43) == "\"Wrong API key.\"")
      {
        Serial.println("Code is successfully updated. This is a new feature.");
        for (int i = 0; i < 2; i++)
        {
          Enable_red_LED();
          delay(250);
          Disable_LED();
          delay(250);
        }
      }
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpsResponseCode);
  }
  // Free resources
  httpsPost.end();
}
/******************************************************************************
  Enable RED LED
*****************************************************************************/
void Enable_red_LED()
{
  digitalWrite(Red, HIGH);
  digitalWrite(Blue, LOW);
}
/******************************************************************************
  Enable BLUE LED
*****************************************************************************/
void Enable_blue_LED()
{
  digitalWrite(Red, LOW);
  digitalWrite(Blue, HIGH);
}
/******************************************************************************
  Disable BOTH LEDs
*****************************************************************************/
void Disable_LED()
{
  digitalWrite(Red, LOW);
  digitalWrite(Blue, LOW);
}
/******************************************************************************
  Card detection indicator
*****************************************************************************/
void Card_detected(int duration)
{
  digitalWrite(Red, HIGH);
  digitalWrite(Blue, HIGH);
  //digitalWrite(Beep, HIGH);
  delay(duration);
  //digitalWrite(Beep, LOW);
  digitalWrite(Red, LOW);
  digitalWrite(Blue, LOW);
}
/******************************************************************************
  OTA Firmware Update
*****************************************************************************/
void FirmwareUpdate()
{
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
  switch (ret)
  {
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
  Beep when OTA update didn't push
*****************************************************************************/
void OTAerror()
{
  digitalWrite(Beep, HIGH);
  delay(400);
  digitalWrite(Beep, LOW);
}
/******************************************************************************
  Extract UID from card and add it to json structure together with other data
*****************************************************************************/
void readcard()
{
  Card_detected(200);
  char UIDstr[32] = "";
  array_to_string(mfrc522.uid.uidByte, 4, UIDstr); //Insert (byte array, length, char array for output)
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  UID = UIDstr;
  Send_live_data(UID, send_location, devname);
}
void initializeDevID()
{
  if (devname == NULL)
  {
    devname = getdevname();
  }
}
String getdevname()
{
  SPIFFS.begin();
  File fileToRead = SPIFFS.open("/iotconfig.txt", "r");
  int i = 0;
  String deviceid;
  while (fileToRead.available())
  {
    deviceid = fileToRead.readStringUntil('\n');
    i++;
    Serial.println(deviceid);
  }
  deviceid.trim();
  fileToRead.close();
  String device_id = deviceid;
  return device_id;
}
