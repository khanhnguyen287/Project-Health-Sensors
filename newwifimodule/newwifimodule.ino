#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>

#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#define USE_NEW_EAP_API
#else
#include "esp_wpa2.h"
#endif

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

// EEPROM memory layout
#define SSID_ADDR      0
#define IDENTITY_ADDR  100
#define USERNAME_ADDR 200
#define PASSWORD_ADDR 300
#define WIFI_MODE_ADDR 400

#define WIFI_PERSONAL   0
#define WIFI_INSTITUTION 1

#define FIELD_SIZE 100

char wifiSSID[FIELD_SIZE];
char wifiIdentity[FIELD_SIZE];
char wifiUsername[FIELD_SIZE];
char wifiPassword[FIELD_SIZE];
int wifiMode = WIFI_INSTITUTION;


// ======================================================
// WiFi connection attempts
// ======================================================
#define MAX_WIFI_RETRY 10

// Each attempt waits approximately 5 seconds
#define WIFI_ATTEMPT_TIMEOUT 5000


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
void writeEEPROMString(
  int startAddr,
  const char* data,
  int maxLength
)
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
void readEEPROMString(
  int startAddr,
  char* buffer,
  int maxLength
)
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
int loadWiFiModeFromEEPROM()
{
  int mode = EEPROM.read(WIFI_MODE_ADDR);

  if (mode != WIFI_PERSONAL && mode != WIFI_INSTITUTION)
  {
    mode = WIFI_INSTITUTION;
  }

  return mode;
}


bool loadWiFiFromEEPROM()
{
  wifiMode = loadWiFiModeFromEEPROM();

  readEEPROMString(
    SSID_ADDR,
    wifiSSID,
    FIELD_SIZE
  );

  readEEPROMString(
    IDENTITY_ADDR,
    wifiIdentity,
    FIELD_SIZE
  );

  readEEPROMString(
    USERNAME_ADDR,
    wifiUsername,
    FIELD_SIZE
  );

  readEEPROMString(
    PASSWORD_ADDR,
    wifiPassword,
    FIELD_SIZE
  );


  Serial.println();
  Serial.println("======================================");
  Serial.println("WIFI CREDENTIALS FROM EEPROM");
  Serial.println("======================================");

  Serial.print("WiFi Type: ");
  Serial.println(wifiMode == WIFI_INSTITUTION ? "Institution WiFi (WPA2-Enterprise)" : "Personal WiFi (WPA2/WPA3-PSK)");

  Serial.print("SSID: ");
  Serial.println(wifiSSID);

  Serial.print("Identity: ");
  Serial.println(wifiIdentity);

  Serial.print("Username: ");
  Serial.println(wifiUsername);

  // Do NOT print the actual password
  Serial.println("Password: ********");

  Serial.println("======================================");


  if (strlen(wifiSSID) == 0)
  {
    return false;
  }

  if (strlen(wifiPassword) == 0)
  {
    return false;
  }

  if (wifiMode == WIFI_INSTITUTION &&
      (strlen(wifiIdentity) == 0 || strlen(wifiUsername) == 0))
  {
    return false;
  }

  return true;
}


// ======================================================
// Save Enterprise WiFi credentials
// ======================================================
void saveWiFiToEEPROM(
  int mode,
  const char* ssid,
  const char* identity,
  const char* username,
  const char* password
)
{
  Serial.println();
  Serial.println("Saving WiFi credentials to EEPROM...");


  EEPROM.write(WIFI_MODE_ADDR, mode);
  EEPROM.commit();

  writeEEPROMString(
    SSID_ADDR,
    ssid,
    FIELD_SIZE
  );

  writeEEPROMString(
    IDENTITY_ADDR,
    identity,
    FIELD_SIZE
  );

  writeEEPROMString(
    USERNAME_ADDR,
    username,
    FIELD_SIZE
  );

  writeEEPROMString(
    PASSWORD_ADDR,
    password,
    FIELD_SIZE
  );


  Serial.println("WiFi credentials saved successfully.");
}


// ======================================================
// Configure Enterprise WiFi
// ======================================================
bool configureEnterpriseWiFi()
{
  Serial.println();
  Serial.println("Configuring WPA2-Enterprise...");
  Serial.println("Authentication: PEAP");


#if defined(USE_NEW_EAP_API)

  esp_err_t result;


  // Identity
  result = esp_eap_client_set_identity(
    (uint8_t*)wifiIdentity,
    strlen(wifiIdentity)
  );

  if (result != ESP_OK)
  {
    Serial.print("Failed to set identity. Error: ");
    Serial.println(result);

    return false;
  }


  // Username
  result = esp_eap_client_set_username(
    (uint8_t*)wifiUsername,
    strlen(wifiUsername)
  );

  if (result != ESP_OK)
  {
    Serial.print("Failed to set username. Error: ");
    Serial.println(result);

    return false;
  }


  // Password
  result = esp_eap_client_set_password(
    (uint8_t*)wifiPassword,
    strlen(wifiPassword)
  );

  if (result != ESP_OK)
  {
    Serial.print("Failed to set password. Error: ");
    Serial.println(result);

    return false;
  }


  // Enable Enterprise WiFi
  Serial.println("Enabling Enterprise WiFi...");
  Serial.print("WiFi mode = ");
  Serial.println(WiFi.getMode());
  result = esp_wifi_sta_enterprise_enable();

  if (result != ESP_OK)
  {
    Serial.print("Failed to enable Enterprise WiFi. Error: ");
    Serial.println(result);

    return false;
  }

#else

  esp_err_t result;


  result = esp_wifi_sta_wpa2_ent_set_identity(
    (uint8_t*)wifiIdentity,
    strlen(wifiIdentity)
  );

  if (result != ESP_OK)
  {
    return false;
  }


  result = esp_wifi_sta_wpa2_ent_set_username(
    (uint8_t*)wifiUsername,
    strlen(wifiUsername)
  );

  if (result != ESP_OK)
  {
    return false;
  }


  result = esp_wifi_sta_wpa2_ent_set_password(
    (uint8_t*)wifiPassword,
    strlen(wifiPassword)
  );

  if (result != ESP_OK)
  {
    return false;
  }


  result = esp_wifi_sta_wpa2_ent_enable();

  if (result != ESP_OK)
  {
    return false;
  }

#endif


  Serial.println("Enterprise WiFi configured.");

  return true;
}


// ======================================================
// Try Enterprise WiFi connection
// Maximum 10 attempts
// ======================================================
bool connectWiFiFromEEPROM()
{
  if (!loadWiFiFromEEPROM())
  {
    Serial.println("No valid WiFi credentials found.");
    return false;
  }

  Serial.println();
  Serial.println("======================================");
  Serial.println("CONNECTING TO WIFI");
  Serial.println("======================================");

  Serial.print("WiFi Type: ");
  Serial.println(wifiMode == WIFI_INSTITUTION ? "Institution WiFi / WPA2-Enterprise" : "Personal WiFi / WPA2-PSK");
  Serial.print("SSID: ");
  Serial.println(wifiSSID);

  WiFi.mode(WIFI_STA);
  delay(1000);

  if (wifiMode == WIFI_INSTITUTION)
  {
    Serial.println("Authentication: PEAP");

    if (!configureEnterpriseWiFi())
    {
      Serial.println("Enterprise configuration failed.");
      return false;
    }
  }
  else
  {
    Serial.println("Authentication: WPA2/WPA3-Personal (PSK)");
  }

  WiFi.begin(wifiSSID, wifiPassword);
  setTrafficLight(false, true, false);

  for (int retry = 1; retry <= MAX_WIFI_RETRY; retry++)
  {
    Serial.println();
    Serial.print("WiFi connection attempt ");
    Serial.print(retry);
    Serial.print("/");
    Serial.println(MAX_WIFI_RETRY);

    unsigned long startTime = millis();

    while (millis() - startTime < WIFI_ATTEMPT_TIMEOUT)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println();
        Serial.println("======================================");
        Serial.println("WIFI CONNECTED!");
        Serial.println("======================================");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
        Serial.println("======================================");

        setTrafficLight(false, false, true);
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

      if (wifiMode == WIFI_INSTITUTION)
      {
        if (!configureEnterpriseWiFi())
        {
          Serial.println("Enterprise configuration failed.");
          return false;
        }
      }

      WiFi.begin(wifiSSID, wifiPassword);
    }
  }

  Serial.println();
  Serial.println("======================================");
  Serial.println("10 WIFI CONNECTION ATTEMPTS FAILED");
  Serial.println("======================================");

  return false;
}

// ======================================================
// Create English Web Page
// ======================================================
String createWebPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-S3 WiFi Setup</title>

<style>
*{box-sizing:border-box}
body{
  margin:0; min-height:100vh; font-family:Arial,Helvetica,sans-serif;
  background:radial-gradient(circle at 10% 10%,rgba(33,150,243,.20),transparent 30%),
             radial-gradient(circle at 90% 20%,rgba(124,77,255,.18),transparent 28%),
             linear-gradient(135deg,#eef7ff 0%,#f8f4ff 50%,#f2fbf8 100%);
  color:#263238; padding:24px 14px 40px;
}
.container{
  width:100%; max-width:520px; margin:0 auto; background:rgba(255,255,255,.97);
  border-radius:24px; overflow:hidden; box-shadow:0 18px 50px rgba(48,63,159,.16);
}
.header{
  position:relative; padding:30px 24px 28px; text-align:center; color:white;
  background:linear-gradient(135deg,#1976d2 0%,#3949ab 52%,#7e57c2 100%);
}
.logo{
  position:relative; z-index:1; width:58px; height:58px; margin:0 auto 13px;
  border-radius:18px; display:flex; align-items:center; justify-content:center;
  background:rgba(255,255,255,.17); border:1px solid rgba(255,255,255,.3); font-size:28px;
}
h1{position:relative;z-index:1;margin:0;font-size:25px}
.subtitle{position:relative;z-index:1;margin:9px 0 0;font-size:14px;color:rgba(255,255,255,.88);line-height:1.5}
.content{padding:24px}
.section-title{margin:0 0 13px;font-size:16px;font-weight:700;color:#263238}
.wifi-options{display:flex;gap:12px;margin:0 0 24px}
.wifi-card{
  flex:1;position:relative;cursor:pointer;border:2px solid #e0e5ec;border-radius:16px;
  padding:17px 10px 15px;text-align:center;background:#fff;transition:.18s;
  box-shadow:0 4px 12px rgba(30,50,80,.05)
}
.wifi-card:hover{transform:translateY(-2px);border-color:#90caf9;box-shadow:0 8px 18px rgba(33,150,243,.12)}
.wifi-card.selected{
  border-color:#2196f3;background:linear-gradient(145deg,#edf7ff,#f5f1ff);
  box-shadow:0 8px 22px rgba(33,150,243,.16)
}
.wifi-card input{position:absolute;opacity:0;pointer-events:none}
.wifi-icon{
  width:42px;height:42px;margin:0 auto 9px;border-radius:13px;display:flex;
  align-items:center;justify-content:center;background:#edf3f9;font-size:21px
}
.wifi-card.selected .wifi-icon{background:linear-gradient(135deg,#2196f3,#7e57c2);color:white}
.wifi-card-title{font-size:15px;font-weight:700;color:#263238}
.wifi-card-description{font-size:11px;color:#78909c;margin-top:5px;line-height:1.35}
.check{
  display:none;position:absolute;right:9px;top:9px;width:20px;height:20px;border-radius:50%;
  background:#2196f3;color:white;font-size:13px;line-height:20px;font-weight:bold
}
.wifi-card.selected .check{display:block}
.field-group{margin-top:17px}
label.field-label{
  display:flex;align-items:center;gap:6px;margin-bottom:7px;font-size:14px;font-weight:700;color:#37474f
}
.required{color:#e53935}
input.text-input{
  width:100%;padding:13px 14px;border:1.5px solid #d7dee7;border-radius:11px;
  background:#fbfcfe;color:#263238;font-size:15px;transition:.18s
}
input.text-input::placeholder{color:#a1adb8}
input.text-input:focus{
  outline:none;border-color:#2196f3;background:#fff;box-shadow:0 0 0 4px rgba(33,150,243,.10)
}
.example{color:#78909c;font-size:12px;margin-top:6px;line-height:1.4}
button{
  width:100%;padding:14px 16px;margin-top:25px;border:0;border-radius:12px;font-size:16px;
  font-weight:700;color:white;background:linear-gradient(135deg,#2196f3,#5e35b1);
  box-shadow:0 8px 18px rgba(63,81,181,.22);cursor:pointer;transition:.18s
}
button:hover{transform:translateY(-1px);box-shadow:0 11px 22px rgba(63,81,181,.28)}
.info-box{
  margin-top:20px;padding:14px 15px;border-radius:13px;background:linear-gradient(135deg,#f1f8ff,#f7f2ff);
  border:1px solid #dce8f7;color:#546e7a;font-size:12px;line-height:1.55
}
.info-title{font-size:13px;font-weight:700;color:#3949ab;margin-bottom:5px}
.footer{text-align:center;padding:0 24px 22px;color:#90a4ae;font-size:11px}
@media(max-width:420px){
  body{padding:12px 9px 28px}.content{padding:20px 18px}.header{padding:26px 18px 24px}
  .wifi-options{gap:8px}.wifi-card{padding:14px 7px 13px}.wifi-card-description{font-size:10px}
}
</style>

<script>
function selectWifiType(type)
{
  const institution = (type === 'institution');

  document.getElementById('institutionCard').classList.toggle('selected', institution);
  document.getElementById('personalCard').classList.toggle('selected', !institution);

  document.getElementById('institution').checked = institution;
  document.getElementById('personal').checked = !institution;

  document.getElementById('enterpriseFields').style.display =
    institution ? 'block' : 'none';

  document.getElementById('identity').required = institution;
  document.getElementById('username').required = institution;

  if (!institution)
  {
    document.getElementById('identity').value = '';
    document.getElementById('username').value = '';
  }
}

window.onload = function()
{
  selectWifiType('institution');
};
</script>

</head>

<body>
<div class="container">

  <div class="header">
    <div class="logo">📶</div>
    <h1>ESP32-S3 WiFi Setup</h1>
    <div class="subtitle">Connect your biomedical device to a WiFi network</div>
  </div>

  <div class="content">
    <div class="section-title">Choose your WiFi network</div>

    <form action="/save" method="POST">

      <div class="wifi-options">
        <label class="wifi-card" id="institutionCard" onclick="selectWifiType('institution')">
          <input type="radio" name="wifi_type" id="institution" value="institution" checked>
          <div class="check">✓</div>
          <div class="wifi-icon">🏫</div>
          <div class="wifi-card-title">Institution wifi</div>
          <div class="wifi-card-description">WPA2-Enterprise / eduroam</div>
        </label>

        <label class="wifi-card" id="personalCard" onclick="selectWifiType('personal')">
          <input type="radio" name="wifi_type" id="personal" value="personal">
          <div class="check">✓</div>
          <div class="wifi-icon">🏠</div>
          <div class="wifi-card-title">Personal wifi</div>
          <div class="wifi-card-description">WPA2 / WPA3-Personal</div>
        </label>
      </div>

      <div class="field-group">
        <label class="field-label" for="ssid">WiFi Network Name (SSID) <span class="required">*</span></label>
        <input class="text-input" type="text" name="ssid" id="ssid"
               placeholder="Example: eduroam or HomeWiFi" required>
      </div>

      <div id="enterpriseFields">
        <div class="field-group">
          <label class="field-label" for="identity">Identity <span class="required">*</span></label>
          <input class="text-input" type="text" name="identity" id="identity"
                 placeholder="username@university.edu">
          <div class="example">Your outer identity for WPA2-Enterprise / PEAP.</div>
        </div>

        <div class="field-group">
          <label class="field-label" for="username">Username <span class="required">*</span></label>
          <input class="text-input" type="text" name="username" id="username"
                 placeholder="username@university.edu">
          <div class="example">Your institution WiFi username.</div>
        </div>
      </div>

      <div class="field-group">
        <label class="field-label" for="password">WiFi Password <span class="required">*</span></label>
        <input class="text-input" type="password" name="password" id="password"
               placeholder="Enter your WiFi password" required>
      </div>

      <button type="submit">Save WiFi Settings →</button>
    </form>

    <div class="info-box">
      <div class="info-title">🔐 About your connection</div>
      <strong>Institution wifi</strong> uses WPA2-Enterprise PEAP, suitable for university or eduroam networks.
      <br><br>
      <strong>Personal wifi</strong> uses a normal WPA2/WPA3-Personal SSID and password.
    </div>
  </div>

  <div class="footer">ESP32-S3 Biomedical IoT Device</div>
</div>
</body></body>
</html>
)rawliteral";

  return html;
}

// ======================================================
// Main Web Page
// ======================================================
void handleRoot()
{
  server.send(
    200,
    "text/html; charset=utf-8",
    createWebPage()
  );
}


// ======================================================
// Receive credentials from Web Interface
// ======================================================
void handleSave()
{
  if (!server.hasArg("wifi_type") ||
      !server.hasArg("ssid") ||
      !server.hasArg("password"))
  {
    server.send(400, "text/plain", "Missing WiFi type, SSID, or password.");
    return;
  }

  String type = server.arg("wifi_type");
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String identity = server.hasArg("identity") ? server.arg("identity") : "";
  String username = server.hasArg("username") ? server.arg("username") : "";

  type.trim();
  ssid.trim();
  password.trim();
  identity.trim();
  username.trim();

  int selectedMode = (type == "personal") ? WIFI_PERSONAL : WIFI_INSTITUTION;

  if (ssid.length() == 0 || password.length() == 0)
  {
    server.send(400, "text/plain", "SSID and password are required.");
    return;
  }

  if (selectedMode == WIFI_INSTITUTION &&
      (identity.length() == 0 || username.length() == 0))
  {
    server.send(400, "text/plain", "Institution WiFi requires identity and username.");
    return;
  }

  Serial.println();
  Serial.println("======================================");
  Serial.println("RECEIVED WIFI CREDENTIALS");
  Serial.println("======================================");
  Serial.print("WiFi Type: ");
  Serial.println(selectedMode == WIFI_INSTITUTION ? "Institution WiFi / WPA2-Enterprise" : "Personal WiFi / WPA2-PSK");
  Serial.print("SSID: ");
  Serial.println(ssid);

  if (selectedMode == WIFI_INSTITUTION)
  {
    Serial.print("Identity: ");
    Serial.println(identity);
    Serial.print("Username: ");
    Serial.println(username);
  }
  Serial.println("Password: ********");
  Serial.println("======================================");

  saveWiFiToEEPROM(
    selectedMode,
    ssid.c_str(),
    identity.c_str(),
    username.c_str(),
    password.c_str()
  );

  String response = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Saved</title><style>body{font-family:Arial;text-align:center;padding:50px;background:#f2f2f2}.box{max-width:500px;margin:auto;padding:30px;border-radius:15px;background:white;box-shadow:0 3px 15px rgba(0,0,0,.15)}h2{color:#2196F3}</style></head>
<body><div class="box"><h2>WiFi Configuration Saved!</h2><p>Your WiFi settings were saved successfully.</p><p>The ESP32-S3 is restarting...</p><p>Please wait a few seconds.</p></div></body></html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", response);
  delay(1500);

  Serial.println();
  Serial.println("======================================");
  Serial.println("RESTARTING ESP32-S3");
  Serial.println("======================================");
  ESP.restart();
}

// ======================================================
// Start Access Point Mode
// ======================================================
void startAPMode()
{
  Serial.println();
  Serial.println("======================================");
  Serial.println("STARTING WIFI SETUP MODE");
  Serial.println("======================================");


  // Red = setup mode / no WiFi connection
  setTrafficLight(true, false, false);


  WiFi.mode(WIFI_AP);


  WiFi.softAPConfig(
    apIP,
    apIP,
    netMsk
  );


  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );


  delay(500);


  Serial.print("Setup WiFi SSID: ");
  Serial.println(AP_SSID);

  Serial.print("Setup WiFi Password: ");
  Serial.println(AP_PASSWORD);

  Serial.print("Setup IP Address: ");
  Serial.println(WiFi.softAPIP());


  // ==================================================
  // DNS Captive Portal
  // ==================================================

  dnsServer.start(
    53,
    "*",
    apIP
  );


  // ==================================================
  // Web Server
  // ==================================================

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/save",
    HTTP_POST,
    handleSave
  );


  // Captive portal detection
  server.on(
    "/generate_204",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/hotspot-detect.html",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/connecttest.txt",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/ncsi.txt",
    HTTP_GET,
    handleRoot
  );


  server.onNotFound(
    handleRoot
  );


  server.begin();


  Serial.println("Web server started.");

  Serial.println();
  Serial.println("Connect your phone/computer to:");
  Serial.println(AP_SSID);

  Serial.println();
  Serial.println("Then open:");
  Serial.println("http://192.168.4.1");
}


// ======================================================
// SETUP
// ======================================================
void setup()
{
  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("ESP32-S3 N16R8");
  Serial.println("EDUROAM WIFI PROVISIONING");
  Serial.println("======================================");


  // ==================================================
  // LED
  // ==================================================

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  setTrafficLight(false, false, false);


  // ==================================================
  // EEPROM
  // ==================================================

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("EEPROM INITIALIZATION FAILED!");
  }
  else
  {
    Serial.println("EEPROM INITIALIZED.");
  }


  // ==================================================
  // Try to connect using EEPROM credentials
  // ==================================================

  bool wifiConnected = connectWiFiFromEEPROM();


  // ==================================================
  // Failed → setup AP
  // ==================================================

  if (!wifiConnected)
  {
    startAPMode();
  }
  else
  {
    Serial.println();
    Serial.println("ESP32-S3 is ready.");
  }
}


// ======================================================
// LOOP
// ======================================================
void loop()
{
  // ==================================================
  // AP MODE
  // ==================================================

  if (WiFi.getMode() == WIFI_AP)
  {
    dnsServer.processNextRequest();

    server.handleClient();
  }


  // ==================================================
  // STA MODE
  // ==================================================

  else if (WiFi.getMode() == WIFI_STA)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      // Red = connection lost
      setTrafficLight(true, false, false);

      Serial.println("WiFi connection lost!");
    }
  }


  delay(10);
}