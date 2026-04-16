#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ============================================================
//  wifi_config.h  –  Quản lý kết nối Wi-Fi cho ESP32
//  Thứ tự ưu tiên:
//    1. Thử Wi-Fi hardcoded trong code
//    2. Thử Wi-Fi đã lưu trong EEPROM
//    3. Phát AP + Captive Portal để người dùng cấu hình
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

// ---------- Cấu hình AP ----------
#define WIFI_AP_SSID      "ESP32_AQM"
#define WIFI_AP_PASS      "12345678"
#define EEPROM_SIZE       96
#define WIFI_TIMEOUT_MS   15000   // Thời gian chờ kết nối tối đa (ms)

// ---------- DNS server để bắt mọi request → captive portal ----------
DNSServer dnsServer;

// ---------- IP tĩnh khi ở chế độ STA ----------
extern IPAddress local_IP;      // Khai báo trong file .ino chính
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;

// ---------- Biến nội bộ ----------
static String _storedSSID = "";
static String _storedPASS = "";
static bool   _apMode     = false;

// ============================================================
//  EEPROM helpers
// ============================================================
static void wifi_saveEEPROM(const String& ssid, const String& pass) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) EEPROM.write(i,      i < (int)ssid.length() ? ssid[i] : 0);
  for (int i = 0; i < 64; i++) EEPROM.write(32 + i, i < (int)pass.length() ? pass[i] : 0);
  EEPROM.commit();
  EEPROM.end();
}

static void wifi_loadEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  char ssid[33] = {}, pass[65] = {};
  for (int i = 0; i < 32; i++) ssid[i] = EEPROM.read(i);
  for (int i = 0; i < 64; i++) pass[i] = EEPROM.read(32 + i);
  EEPROM.end();
  _storedSSID = String(ssid);
  _storedPASS = String(pass);
}

// ============================================================
//  Hàm kết nối STA (trả về true nếu thành công)
// ============================================================
static bool wifi_connectSTA(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("[WiFi] Lỗi cấu hình IP tĩnh");
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WiFi] Đang kết nối: %s ", ssid.c_str());

  unsigned long t = millis();
  while (millis() - t < WIFI_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WiFi] ✅ Kết nối thành công! IP: %s\n",
                    WiFi.localIP().toString().c_str());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] ❌ Thất bại.");
  return false;
}

// ============================================================
//  HTML Captive Portal
//  - Quét WiFi hiện có, cho nhập mật khẩu
//  - Hiện thông báo địa chỉ web chính TRƯỚC khi gửi về ESP32
// ============================================================
static const char WIFI_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Cấu hình Wi-Fi</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .card {
    background: rgba(255,255,255,0.08);
    backdrop-filter: blur(12px);
    border: 1px solid rgba(255,255,255,0.15);
    border-radius: 16px;
    padding: 32px 28px;
    width: 320px;
    color: #fff;
    box-shadow: 0 8px 32px rgba(0,0,0,0.4);
  }
  h2 {
    font-size: 1.3rem;
    margin-bottom: 6px;
    color: #00d4ff;
  }
  p.sub {
    font-size: 0.8rem;
    color: rgba(255,255,255,0.5);
    margin-bottom: 20px;
  }
  label {
    display: block;
    font-size: 0.78rem;
    color: rgba(255,255,255,0.7);
    margin-bottom: 4px;
  }
  select, input[type=password] {
    width: 100%;
    padding: 10px 12px;
    border-radius: 8px;
    border: 1px solid rgba(255,255,255,0.2);
    background: rgba(255,255,255,0.07);
    color: #fff;
    font-size: 0.9rem;
    margin-bottom: 14px;
    outline: none;
    transition: border .2s;
  }
  select:focus, input:focus { border-color: #00d4ff; }
  select option { background: #1a1a2e; color: #fff; }
  button {
    width: 100%;
    padding: 11px;
    background: linear-gradient(90deg, #00d4ff, #0077ff);
    color: #fff;
    border: none;
    border-radius: 8px;
    font-size: 0.95rem;
    font-weight: 600;
    cursor: pointer;
    transition: opacity .2s;
  }
  button:disabled { opacity: 0.5; cursor: not-allowed; }
  button:hover:not(:disabled) { opacity: 0.85; }
  #status {
    margin-top: 14px;
    font-size: 0.8rem;
    color: rgba(255,255,255,0.55);
    min-height: 18px;
    text-align: center;
  }
  /* Modal thông báo IP */
  .modal-overlay {
    display: none;
    position: fixed; inset: 0;
    background: rgba(0,0,0,0.7);
    align-items: center;
    justify-content: center;
    z-index: 999;
  }
  .modal-overlay.show { display: flex; }
  .modal {
    background: #162032;
    border: 1px solid #00d4ff;
    border-radius: 14px;
    padding: 28px 24px;
    max-width: 280px;
    text-align: center;
    color: #fff;
    box-shadow: 0 4px 24px rgba(0,212,255,0.2);
  }
  .modal h3 { color: #00d4ff; margin-bottom: 10px; font-size: 1rem; }
  .modal p { font-size: 0.88rem; line-height: 1.5; color: rgba(255,255,255,0.8); }
  .ip-highlight {
    display: inline-block;
    margin: 10px 0;
    padding: 6px 16px;
    background: rgba(0,212,255,0.15);
    border: 1px solid #00d4ff;
    border-radius: 6px;
    font-size: 1rem;
    font-weight: bold;
    color: #00d4ff;
    letter-spacing: 0.5px;
  }
  .modal button {
    margin-top: 16px;
    background: linear-gradient(90deg, #00d4ff, #0077ff);
    width: auto;
    padding: 9px 28px;
    font-size: 0.9rem;
  }
</style>
</head>
<body>
<div class="card">
  <h2>&#x1F4F6; Cấu hình Wi-Fi</h2>
  <p class="sub">Chọn mạng Wi-Fi và nhập mật khẩu</p>

  <label>Mạng Wi-Fi</label>
  <select id="ssidSelect">
    <option value="">⏳ Đang quét mạng...</option>
  </select>

  <label>Mật khẩu</label>
  <input type="password" id="passInput" placeholder="Nhập mật khẩu">

  <button id="btnConnect" onclick="showConfirmModal()" disabled>Kết nối</button>
  <div id="status"></div>
</div>

<!-- Modal xác nhận & thông báo IP -->
<div class="modal-overlay" id="confirmModal">
  <div class="modal">
    <h3>&#x2139;&#xFE0F; Thông tin truy cập</h3>
    <p>Sau khi ESP32 kết nối Wi-Fi, bạn có thể truy cập bảng điều khiển tại:</p>
    <div class="ip-highlight">http://192.168.1.4</div>
    <p>Nhấn <strong>OK</strong> để xác nhận gửi thông tin Wi-Fi về thiết bị.</p>
    <button onclick="submitWiFi()">OK</button>
  </div>
</div>

<script>
// Quét và hiển thị danh sách Wi-Fi
function loadWiFiList() {
  document.getElementById('status').textContent = 'Đang quét mạng Wi-Fi...';
  fetch('/scanwifi')
    .then(r => r.json())
    .then(list => {
      const sel = document.getElementById('ssidSelect');
      sel.innerHTML = '';
      if (list.length === 0) {
        sel.innerHTML = '<option value="">Không tìm thấy mạng nào</option>';
        document.getElementById('status').textContent = 'Không tìm thấy mạng.';
        return;
      }
      list.forEach(s => {
        const opt = document.createElement('option');
        opt.value = s;
        opt.textContent = s;
        sel.appendChild(opt);
      });
      document.getElementById('btnConnect').disabled = false;
      document.getElementById('status').textContent =
        'Tìm thấy ' + list.length + ' mạng. Chọn và nhập mật khẩu.';
    })
    .catch(() => {
      document.getElementById('status').textContent = 'Lỗi quét mạng. Thử lại sau...';
      setTimeout(loadWiFiList, 3000);
    });
}

// Hiện modal thông báo IP trước khi gửi
function showConfirmModal() {
  const ssid = document.getElementById('ssidSelect').value;
  if (!ssid) {
    document.getElementById('status').textContent = 'Vui lòng chọn mạng Wi-Fi.';
    return;
  }
  document.getElementById('confirmModal').classList.add('show');
}

// Gửi thông tin Wi-Fi về ESP32 sau khi người dùng bấm OK
function submitWiFi() {
  document.getElementById('confirmModal').classList.remove('show');

  const ssid = document.getElementById('ssidSelect').value;
  const pass = document.getElementById('passInput').value;
  const btn  = document.getElementById('btnConnect');

  btn.disabled = true;
  document.getElementById('status').textContent = 'Đang gửi thông tin...';

  fetch('/savewifi?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass))
    .then(r => r.text())
    .then(msg => {
      document.getElementById('status').textContent = msg;
    })
    .catch(() => {
      // ESP32 restart → mất kết nối AP là bình thường
      document.getElementById('status').textContent =
        '✅ Đã gửi! Thiết bị đang khởi động lại...';
    });
}

loadWiFiList();
</script>
</body>
</html>
)rawliteral";


// ============================================================
//  Khởi động chế độ AP + Captive Portal
// ============================================================
static void wifi_startAP(WebServer& server) {
  _apMode = true;

  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

  Serial.printf("[WiFi] 📡 AP Mode: SSID=%s  Pass=%s  IP=%s\n",
                WIFI_AP_SSID, WIFI_AP_PASS,
                WiFi.softAPIP().toString().c_str());

  // DNS: bắt toàn bộ domain về 192.168.4.1 → captive portal tự bật
  dnsServer.start(53, "*", apIP);

  // Route captive portal
  auto sendPortal = [&server]() {
    server.send_P(200, "text/html", WIFI_CONFIG_PAGE);
  };

  server.on("/",                 sendPortal);
  server.on("/generate_204",     sendPortal);   // Android
  server.on("/fwlink",           sendPortal);   // Windows
  server.on("/hotspot-detect.html", sendPortal);// iOS/macOS
  server.onNotFound(             sendPortal);   // Mọi URL khác

  // Quét danh sách Wi-Fi xung quanh
  server.on("/scanwifi", [&server]() {
    int n = WiFi.scanNetworks(false, true);
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i) json += ",";
      // Escape tên mạng có dấu nháy đôi
      String name = WiFi.SSID(i);
      name.replace("\"", "\\\"");
      json += "\"" + name + "\"";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // Lưu Wi-Fi mới và restart
  server.on("/savewifi", [&server]() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      String newSSID = server.arg("ssid");
      String newPASS = server.arg("pass");
      wifi_saveEEPROM(newSSID, newPASS);
      server.send(200, "text/plain",
                  "✅ Đã lưu Wi-Fi \"" + newSSID + "\". Đang khởi động lại...");
      delay(1500);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "❌ Thiếu tham số ssid hoặc pass");
    }
  });
}

// ============================================================
//  Hàm xử lý DNS trong loop() khi đang ở AP mode
// ============================================================
static void wifi_handleDNS() {
  if (_apMode) dnsServer.processNextRequest();
}

// ============================================================
//  Hàm chính – gọi trong setup() thay cho connect_WiFi()
//
//  Tham số:
//    hardSSID / hardPASS : Wi-Fi cứng trong code
//    server              : WebServer đã khai báo trong .ino
//
//  Trả về: true nếu kết nối STA thành công
// ============================================================
static bool wifi_init(const char* hardSSID, const char* hardPASS,
                      WebServer& server) {

  // --- Bước 1: Thử Wi-Fi hardcoded ---
  Serial.println("[WiFi] Bước 1: Thử Wi-Fi cứng trong code...");
  if (wifi_connectSTA(String(hardSSID), String(hardPASS))) return true;

  // --- Bước 2: Thử Wi-Fi lưu trong EEPROM ---
  Serial.println("[WiFi] Bước 2: Thử Wi-Fi đã lưu trong EEPROM...");
  wifi_loadEEPROM();
  if (wifi_connectSTA(_storedSSID, _storedPASS)) return true;

  // --- Bước 3: Phát AP + Captive Portal ---
  Serial.println("[WiFi] Bước 3: Không có Wi-Fi → Chạy AP Config...");
  wifi_startAP(server);
  return false;
}

#endif // WIFI_CONFIG_H