#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>          // Lưu thông tin WiFi
#include <ESP8266WebServer.h> // Web server cho AP config
#include <DNSServer.h>       // Captive portal DNS
#include "wifi_config.h"     // Trang cấu hình WiFi

// --- CẤU HÌNH ---
const char* ssid     = "YOUR WIFI";
const char* password = "YOUR WIFI PASSWORD";

// Địa chỉ IP của ESP8266 server (phải khớp với file esp32_breeziphat.ino)
const char* serverIP = "192.168.1.4";

// Chân GPIO để điều khiển
const int FILTER_FAN_PIN = 0; // GPIO0

// --- BIẾN TOÀN CỤC ĐỂ LƯU TRẠNG THÁI TỪ SERVER ---
String filterFanMode = "off"; // Giá trị mặc định khi khởi động
int currentAQI = -1;          // -1 có nghĩa là chưa nhận được dữ liệu

// --- BIẾN WIFI ---
#define EEPROM_SIZE   96
#define WIFI_TIMEOUT  15000  // ms

const char* ap_ssid = "ESP8266_Config";
const char* ap_pass = "12345678";

String storedSSID = "";
String storedPASS = "";
bool   apMode     = false;

ESP8266WebServer apServer(80);

// ============================================================
//  EEPROM helpers
// ============================================================
void saveWiFiConfig(String s, String p) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) EEPROM.write(i,      i < (int)s.length() ? s[i] : 0);
  for (int i = 0; i < 64; i++) EEPROM.write(32 + i, i < (int)p.length() ? p[i] : 0);
  EEPROM.commit();
  EEPROM.end();
}

void loadWiFiConfig() {
  EEPROM.begin(EEPROM_SIZE);
  char s[33] = {}, p[65] = {};
  for (int i = 0; i < 32; i++) s[i] = EEPROM.read(i);
  for (int i = 0; i < 64; i++) p[i] = EEPROM.read(32 + i);
  EEPROM.end();
  storedSSID = String(s);
  storedPASS = String(p);
}

// ============================================================
//  Kết nối STA, trả về true nếu thành công
// ============================================================
bool connectWiFi(String s, String p) {
  if (s.length() == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(s.c_str(), p.c_str());
  Serial.printf("Connecting WiFi: %s ", s.c_str());

  unsigned long start = millis();
  while (millis() - start < WIFI_TIMEOUT) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nWiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Failed.");
  return false;
}

// ============================================================
//  Khởi động AP + Captive Portal
// ============================================================
void startAPConfig() {
  apMode = true;
  IPAddress apIP(192, 168, 4, 1);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.printf("AP Mode → SSID: %s  Pass: %s  IP: %s\n",
                ap_ssid, ap_pass, WiFi.softAPIP().toString().c_str());

  // DNS bắt toàn bộ domain → captive portal tự popup trên iOS/Android/Windows
  dnsServer.start(53, "*", apIP);

  auto sendPortal = []() { apServer.send_P(200, "text/html", configPage); };

  apServer.on("/",                    sendPortal);
  apServer.on("/generate_204",        sendPortal);  // Android
  apServer.on("/fwlink",              sendPortal);  // Windows
  apServer.on("/hotspot-detect.html", sendPortal);  // iOS / macOS
  apServer.onNotFound(                sendPortal);  // Mọi URL khác

  // Quét danh sách WiFi xung quanh
  apServer.on("/scanwifi", []() {
    int n = WiFi.scanNetworks(false, true);
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i) json += ",";
      String name = WiFi.SSID(i);
      name.replace("\"", "\\\"");
      json += "\"" + name + "\"";
    }
    json += "]";
    apServer.send(200, "application/json", json);
  });

  // Lưu WiFi mới vào EEPROM rồi restart
  apServer.on("/savewifi", []() {
    if (apServer.hasArg("ssid") && apServer.hasArg("pass")) {
      saveWiFiConfig(apServer.arg("ssid"), apServer.arg("pass"));
      apServer.send(200, "text/plain", "Lưu thành công. ESP sẽ khởi động lại.");
      delay(1000);
      ESP.restart();
    } else {
      apServer.send(400, "text/plain", "Thiếu tham số");
    }
  });

  apServer.begin();
}

// ============================================================
//  Hàm WiFi chính – gọi trong setup()
// ============================================================
void connect_WiFi() {
  // --- Bước 1: Thử WiFi hardcoded trong code ---
  Serial.println("[WiFi] Bước 1: Thử WiFi cứng trong code...");
  if (connectWiFi(String(ssid), String(password))) return;

  // --- Bước 2: Thử WiFi đã lưu trong EEPROM ---
  Serial.println("[WiFi] Bước 2: Thử WiFi đã lưu trong EEPROM...");
  loadWiFiConfig();
  if (connectWiFi(storedSSID, storedPASS)) return;

  // --- Bước 3: Phát AP + Captive Portal ---
  Serial.println("[WiFi] Bước 3: Không có WiFi → Chạy AP Config...");
  startAPConfig();
}


void setup() {
  Serial.begin(115200);
  delay(100);

  // Cấu hình chân GPIO là OUTPUT, mặc định tắt
  pinMode(FILTER_FAN_PIN, OUTPUT);
  digitalWrite(FILTER_FAN_PIN, LOW);

  // Kết nối WiFi (hardcoded → EEPROM → AP)
  connect_WiFi();
}


// Hàm này kết nối tới server và cập nhật tất cả trạng thái (AQI, fan, mist)
void updateStateFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  
  String serverUrl = "http://" + String(serverIP) + "/data";
  
  if (http.begin(client, serverUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
      } else {
        // Cập nhật các biến toàn cục với dữ liệu mới từ server
        currentAQI = doc["aqi"];
        filterFanMode = doc["fanMode"].as<String>();
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTP] Unable to connect\n");
  }
}

void loop() {
  // Xử lý DNS và web server khi đang ở AP mode
  if (apMode) {
    dnsServer.processNextRequest();
    apServer.handleClient();
    return; // Khi đang AP mode thì không chạy logic bên dưới
  }

  // 1. Lấy trạng thái mới nhất từ server
  updateStateFromServer();

  // 2. Xử lý logic cho Filter Fan dựa trên trạng thái vừa nhận được
  Serial.printf("Server state: Fan=%s, AQI=%d\n", filterFanMode.c_str(), currentAQI);

  if (filterFanMode == "on") {
    digitalWrite(FILTER_FAN_PIN, LOW);
    Serial.println("-> Filter Fan: ON ");
  } else if (filterFanMode == "off") {
    digitalWrite(FILTER_FAN_PIN, HIGH);
    Serial.println("-> Filter Fan: OFF ");
  } else if (filterFanMode == "auto") {
    if (currentAQI > 100) {
      digitalWrite(FILTER_FAN_PIN, LOW);
      Serial.println("-> Filter Fan (Auto): ON because AQI > 100");
    } else {
      digitalWrite(FILTER_FAN_PIN, HIGH);
      Serial.println("-> Filter Fan (Auto): OFF because AQI <= 100");
    }
  }

  Serial.println("------------------------------------");
  delay(500); // Chờ 2 giây cho lần lặp tiếp theo
}
