#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h> // Included for standard C time functions

#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#define USE_NEW_EAP_API
#else
#include "esp_wpa2.h"
#endif

// ======================================================
// NTP Configuration
// ======================================================
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 0;          // Set offset in seconds (e.g., 7*3600 for UTC+7, 0 for UTC)
const int   DAYLIGHT_OFFSET_SEC = 0;     // Set daylight saving offset in seconds if needed

// ======================================================
// LED GPIO
// ======================================================
const int RED_PIN    = 4;
const int YELLOW_PIN = 5;
const int GREEN_PIN  = 6;

// ======================================================
// ESP32-S3 Setup Access Point
// ======================================================
const char* AP_SSID = "ESP32S3-SETUP";
const char* AP_PASSWORD = "12345678";

// ======================================================
// Google Sheets
// ======================================================
const char* GOOGLE_SCRIPT_URL =
  "https://script.google.com/macros/s/AKfycbz8aiGpbR82ly2Dryy5G2_D6pbBa1UjNlwNgj71RVf-hY0naVidmhZYJGQDYgnTnTg/exec";

const char* API_KEY =
  "ESP32_BIOMEDICAL_2026";

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

// ======================================================
// Web / DNS
// ======================================================
DNSServer dnsServer;
WebServer server(80);

// ======================================================
// EEPROM
// ======================================================
#define EEPROM_SIZE 512

#define SSID_ADDR      0
#define IDENTITY_ADDR  100
#define USERNAME_ADDR 200
#define PASSWORD_ADDR 300

#define FIELD_SIZE 100

char wifiSSID[FIELD_SIZE];
char wifiIdentity[FIELD_SIZE];
char wifiUsername[FIELD_SIZE];
char wifiPassword[FIELD_SIZE];

// ======================================================
// WiFi connection attempts
// ======================================================
#define MAX_WIFI_RETRY 10
#define WIFI_ATTEMPT_TIMEOUT 5000

// ======================================================
// Helper: Get Formatted Local Time
// ======================================================
String getISO8601Time()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000))
  {
    Serial.println("Failed to obtain time. Falling back to timestamp 0.");
    return "1970-01-01T00:00:00Z";
  }

  char timeStringBuff[35];
  // Formats time as "YYYY-MM-DDTHH:MM:SS"
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeStringBuff);
}

// ======================================================
// Traffic Light
// ======================================================
void setTrafficLight(bool red, bool yellow, bool green)
{
  digitalWrite(RED_PIN, red);
  digitalWrite(YELLOW_PIN, yellow);
  digitalWrite(GREEN_PIN, green);
}

// ======================================================
// Clear EEPROM string
// ======================================================
void clearEEPROMString(int startAddr, int maxLength)
{
  for (int i = 0; i < maxLength; i++)
  {
    EEPROM.write(startAddr + i, 0);
  }
}

// ======================================================
// Write string to EEPROM
// ======================================================
void writeEEPROMString(int startAddr, const char* data, int maxLength)
{
  clearEEPROMString(startAddr, maxLength);
  int len = strlen(data);
  if (len >= maxLength)
  {
    len = maxLength - 1;
  }
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(startAddr + i, data[i]);
  }
  EEPROM.write(startAddr + len, '\0');
  EEPROM.commit();
}

// ======================================================
// Read string from EEPROM
// ======================================================
void readEEPROMString(int startAddr, char* buffer, int maxLength)
{
  for (int i = 0; i < maxLength - 1; i++)
  {
    buffer[i] = EEPROM.read(startAddr + i);
    if (buffer[i] == '\0' || buffer[i] == 0xFF)
    {
      buffer[i] = '\0';
      break;
    }
  }
  buffer[maxLength - 1] = '\0';
}

// ======================================================
// Load Enterprise WiFi credentials
// ======================================================
bool loadWiFiFromEEPROM()
{
  readEEPROMString(SSID_ADDR, wifiSSID, FIELD_SIZE);
  readEEPROMString(IDENTITY_ADDR, wifiIdentity, FIELD_SIZE);
  readEEPROMString(USERNAME_ADDR, wifiUsername, FIELD_SIZE);
  readEEPROMString(PASSWORD_ADDR, wifiPassword, FIELD_SIZE);

  Serial.println();
  Serial.println("======================================");
  Serial.println("WIFI CREDENTIALS FROM EEPROM");
  Serial.println("======================================");
  Serial.print("SSID: "); Serial.println(wifiSSID);
  Serial.print("Identity: "); Serial.println(wifiIdentity);
  Serial.print("Username: "); Serial.println(wifiUsername);
  Serial.println("Password: ********");
  Serial.println("======================================");

  if (strlen(wifiSSID) == 0 || strlen(wifiUsername) == 0 || strlen(wifiPassword) == 0)
  {
    return false;
  }

  return true;
}

// ======================================================
// Save Enterprise WiFi credentials
// ======================================================
void saveWiFiToEEPROM(const char* ssid, const char* identity, const char* username, const char* password)
{
  Serial.println("\nSaving WiFi credentials to EEPROM...");
  writeEEPROMString(SSID_ADDR, ssid, FIELD_SIZE);
  writeEEPROMString(IDENTITY_ADDR, identity, FIELD_SIZE);
  writeEEPROMString(USERNAME_ADDR, username, FIELD_SIZE);
  writeEEPROMString(PASSWORD_ADDR, password, FIELD_SIZE);
  Serial.println("WiFi credentials saved successfully.");
}

// ======================================================
// Configure Enterprise WiFi
// ======================================================
bool configureEnterpriseWiFi()
{
  Serial.println("\nConfiguring WPA2-Enterprise...\nAuthentication: PEAP");

#if defined(USE_NEW_EAP_API)
  esp_err_t result;
  result = esp_eap_client_set_identity((uint8_t*)wifiIdentity, strlen(wifiIdentity));
  if (result != ESP_OK) return false;

  result = esp_eap_client_set_username((uint8_t*)wifiUsername, strlen(wifiUsername));
  if (result != ESP_OK) return false;

  result = esp_eap_client_set_password((uint8_t*)wifiPassword, strlen(wifiPassword));
  if (result != ESP_OK) return false;

  result = esp_wifi_sta_enterprise_enable();
  if (result != ESP_OK) return false;
#else
  esp_err_t result;
  result = esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)wifiIdentity, strlen(wifiIdentity));
  if (result != ESP_OK) return false;

  result = esp_wifi_sta_wpa2_ent_set_username((uint8_t*)wifiUsername, strlen(wifiUsername));
  if (result != ESP_OK) return false;

  result = esp_wifi_sta_wpa2_ent_set_password((uint8_t*)wifiPassword, strlen(wifiPassword));
  if (result != ESP_OK) return false;

  result = esp_wifi_sta_wpa2_ent_enable();
  if (result != ESP_OK) return false;
#endif

  Serial.println("Enterprise WiFi configured.");
  return true;
}

// ======================================================
// Try Enterprise WiFi connection
// ======================================================
bool connectWiFiFromEEPROM()
{
  if (!loadWiFiFromEEPROM())
  {
    Serial.println("No valid WiFi credentials found.");
    return false;
  }

  Serial.println("\n======================================");
  Serial.println("CONNECTING TO EDUROAM");
  Serial.println("======================================");

  WiFi.mode(WIFI_STA);
  delay(1000);

  if (!configureEnterpriseWiFi())
  {
    Serial.println("Enterprise configuration failed.");
    return false;
  }

  WiFi.begin(wifiSSID);
  setTrafficLight(false, true, false);

  for (int retry = 1; retry <= MAX_WIFI_RETRY; retry++)
  {
    Serial.print("\nWiFi connection attempt ");
    Serial.print(retry);
    Serial.print("/");
    Serial.println(MAX_WIFI_RETRY);

    unsigned long startTime = millis();
    while (millis() - startTime < WIFI_ATTEMPT_TIMEOUT)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\n======================================");
        Serial.println("EDUROAM CONNECTED!");
        Serial.println("======================================");
        Serial.print("IP Address: "); Serial.println(WiFi.localIP());
        Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
        Serial.println("======================================");

        setTrafficLight(false, false, true);

        // Sync Time via NTP once connected
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        Serial.println("Syncing NTP Time...");

        return true;
      }
      delay(100);
    }

    Serial.println("Connection attempt failed.");

    if (retry < MAX_WIFI_RETRY)
    {
      Serial.println("Retrying...");
      WiFi.disconnect();
      delay(500);

      if (!configureEnterpriseWiFi()) return false;
      WiFi.begin(wifiSSID);
    }
  }

  Serial.println("\n10 WIFI CONNECTION ATTEMPTS FAILED");
  return false;
}

// ======================================================
// Create Web Page
// ======================================================
String createWebPage()
{
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-S3 WiFi Setup</title>
<style>
body { font-family: Arial, sans-serif; background: #f2f2f2; margin: 0; padding: 20px; }
.container { max-width: 500px; margin: auto; background: white; padding: 25px; border-radius: 15px; box-shadow: 0 3px 15px rgba(0,0,0,0.15); }
h1 { text-align: center; color: #333; }
.subtitle { text-align: center; color: #666; margin-bottom: 25px; }
label { display: block; margin-top: 16px; font-weight: bold; color: #333; }
input { width: 100%; padding: 12px; margin-top: 7px; box-sizing: border-box; border: 1px solid #ccc; border-radius: 8px; font-size: 16px; }
button { width: 100%; padding: 13px; margin-top: 25px; border: none; border-radius: 8px; font-size: 16px; background: #2196F3; color: white; cursor: pointer; }
.info { background: #e3f2fd; padding: 14px; border-radius: 8px; margin-bottom: 20px; line-height: 1.5; }
.warning { background: #fff3cd; padding: 14px; border-radius: 8px; margin-top: 20px; color: #856404; line-height: 1.5; }
.example { color: #777; font-size: 13px; margin-top: 5px; }
</style>
</head>
<body>
<div class="container">
<h1>ESP32-S3 WiFi Setup</h1>
<div class="subtitle">WPA2-Enterprise / eduroam</div>
<div class="info">Enter your eduroam credentials below.</div>
<form action="/save" method="POST">
<label for="ssid">WiFi Network Name (SSID)</label>
<input type="text" name="ssid" id="ssid" value="eduroam" required>
<label for="identity">Identity</label>
<input type="text" name="identity" id="identity" placeholder="username@university.edu" required>
<label for="username">Username</label>
<input type="text" name="username" id="username" placeholder="username@university.edu" required>
<label for="password">Password</label>
<input type="password" name="password" id="password" required>
<button type="submit">Save and Restart</button>
</form>
</div>
</body>
</html>
)rawliteral";
}

void handleRoot()
{
  server.send(200, "text/html; charset=utf-8", createWebPage());
}

void handleSave()
{
  if (!server.hasArg("ssid") || !server.hasArg("identity") || !server.hasArg("username") || !server.hasArg("password"))
  {
    server.send(400, "text/plain", "Missing WiFi credentials.");
    return;
  }

  String ssid = server.arg("ssid");
  String identity = server.arg("identity");
  String username = server.arg("username");
  String password = server.arg("password");

  ssid.trim(); identity.trim(); username.trim();

  if (ssid.length() == 0 || identity.length() == 0 || username.length() == 0 || password.length() == 0)
  {
    server.send(400, "text/plain", "All fields are required.");
    return;
  }

  saveWiFiToEEPROM(ssid.c_str(), identity.c_str(), username.c_str(), password.c_str());

  String response = "<html><body><h2>WiFi Saved! Restarting...</h2></body></html>";
  server.send(200, "text/html; charset=utf-8", response);
  delay(1500);
  ESP.restart();
}

void startAPMode()
{
  setTrafficLight(true, false, false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  dnsServer.start(53, "*", apIP);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  server.begin();
}

// ======================================================
// Send Random Data To Google Sheets
// ======================================================
void sendRandomDataToGoogleSheets()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected. Cannot send data.");
    return;
  }

  int heartRate = random(60, 100);  // Random HR between 60-99 BPM
  int spo2 = random(95, 100);       // Random SpO2 between 95-99%
  int rrMs = random(600, 1000);     // Random RR interval between 600-999 ms

  // Retrieve current local ISO string
  String timeISO = getISO8601Time();

  Serial.println("\n======================================");
  Serial.println("SENDING RANDOM DATA TO GOOGLE SHEETS");
  Serial.println("======================================");
  Serial.print("Heart Rate: "); Serial.print(heartRate); Serial.println(" BPM");
  Serial.print("SpO2: "); Serial.print(spo2); Serial.println(" %");
  Serial.print("RR: "); Serial.print(rrMs); Serial.println(" ms");
  Serial.print("Timestamp: "); Serial.println(timeISO);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (!http.begin(client, GOOGLE_SCRIPT_URL))
  {
    Serial.println("HTTP connection failed.");
    return;
  }

  http.addHeader("Content-Type", "application/json");

// Updated JSON string without timestamp
  String jsonPayload = "{\"api_key\":\"ESP32_BIOMEDICAL_2026\",\"heart_rate\":" + String(heartRate) + 
                     ",\"spo2\":" + String(spo2) + 
                     ",\"rr_ms\":" + String(rrMs) + "}";

  Serial.println("JSON:");
  Serial.println(jsonPayload);

  int httpCode = http.POST(jsonPayload);
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);

  if (httpCode > 0)
  {
    String response = http.getString();
    Serial.println("Google Sheets Response:");
    Serial.println(response);
  }
  else
  {
    Serial.print("HTTP Error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
  Serial.println("======================================");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  setTrafficLight(false, false, false);

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("EEPROM INITIALIZATION FAILED!");
  }

  bool wifiConnected = connectWiFiFromEEPROM();

  if (!wifiConnected)
  {
    startAPMode();
  }
}

void loop()
{
  if (WiFi.getMode() == WIFI_AP)
  {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(GREEN_PIN, HIGH);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(RED_PIN, LOW);

    static unsigned long lastSendTime = 0;
    if (lastSendTime == 0 || millis() - lastSendTime >= 5000UL)
    {
      lastSendTime = millis();
      sendRandomDataToGoogleSheets();
    }
  }
  else
  {
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, HIGH);
    digitalWrite(RED_PIN, LOW);

    if (!connectWiFiFromEEPROM())
    {
      startAPMode();
    }
  }
}