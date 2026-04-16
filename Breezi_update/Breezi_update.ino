#define LGFX_USE_V1
#include <LovyanGFX.hpp>     // Thư viện điều khiển màn hình TFT (hiệu năng cao)
#include <lvgl.h>            // Thư viện GUI LVGL
#include <WiFi.h>            // Kết nối WiFi
#include <HTTPClient.h>      // Gửi request HTTP (lấy weather)
#include <ArduinoJson.h>     // Xử lý JSON
#include <Wire.h>            // I2C communication
#include <Adafruit_SHT31.h>  // Cảm biến nhiệt độ/độ ẩm SHT31
#include <images.h>          // Hình ảnh dùng cho UI
#include <WebServer.h>       // Web server nội bộ ESP32
#include "PMS.h"             // Cảm biến bụi mịn PMS
#include <EEPROM.h>          // Lưu thông tin WiFi
#include <DNSServer.h>       // Captive portal DNS

#include "web.h"  // HTML dashboard
#include "ui.h"   // UI objects LVGL
#include "screens.h"
#include "wifi_config.h"  // Trang cấu hình WiFi

//----------------------------------------DEFINES
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Buffer dùng để render LVGL (giảm RAM bằng cách chia nhỏ frame)
#define BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)
uint8_t* draw_buf;

// WIFI & API
const char* ssid = "YOUR WIFI";
const char* password = "YOUR WIFI PASSWORD";

// API OpenWeatherMap
String openWeatherMapApiKey = "YOUR API";
String city = "Saigon";
String countryCode = "VN";

// --- CẤU HÌNH IP TĨNH ---
IPAddress local_IP(192, 168, 1, 4);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Khởi tạo WebServer port 80
WebServer server(80);
bool connected = false;
String fanMode = "auto";

// --- CẤU HÌNH CẢM BIẾN ---
PMS pms(Serial2);                         // Cảm biến bụi dùng UART2
PMS::DATA pmsData;                        // Struct chứa dữ liệu PMS
Adafruit_SHT31 sht31 = Adafruit_SHT31();  // Cảm biến nhiệt độ/độ ẩm

// Các chân analog đọc cảm biến khí
#define PIN_MQ135 33
#define PIN_MQ131 32
#define PIN_CJMCU_CO 35
#define PIN_CJMCU_NO2 39
#define PIN_CJMCU_NH3 34
#define PIN_VOC_EXT 36

// Struct lưu toàn bộ dữ liệu sensor + weather
struct {
  float temp = 0;
  float humi = 0;
  int pm25 = 0;
  int pm10 = 0;
  int co2 = 0;
  int o3 = 0;
  int co = 0;
  int no2 = 0;
  int nh3 = 0;
  int voc = 0;
  int aqi = 0;
  String status = "INIT";
  String pollutant = "PM2.5";
  int pressure = 0;
  String weather = "--";
} sensorData;

// Timer dùng millis (tránh delay blocking)
unsigned long prevMillSensor = 0;
unsigned long prevMillWeather = 0;

// Chu kỳ đọc sensor và weather
#define INTERVAL_SENSOR 10000    // 10s
#define INTERVAL_WEATHER 600000  // 10 phút

//----------------------------------------DRIVER (LovyanGFX)
// Custom class cấu hình TFT
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      // Cấu hình SPI bus
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      // Chân SPI
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = 19;
      cfg.pin_dc = 2;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      // Cấu hình panel TFT
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 5;
      cfg.pin_rst = 4;
      cfg.pin_busy = -1;

      cfg.panel_width = 240;
      cfg.panel_height = 320;

      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;

      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;

      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};
LGFX tft;  // Object TFT

//----------------------------------------LVGL FLUSH
// Callback để LVGL vẽ pixel ra màn hình
void my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((uint16_t*)px_map, w * h, true);
  tft.endWrite();

  // Báo LVGL là vẽ xong
  lv_display_flush_ready(disp);
}

// Callback tick cho LVGL
static uint32_t my_tick_get_cb(void) {
  return millis();
}

//----------------------------------------INIT DISPLAY
void init_Display_System() {
  tft.init();              // Khởi tạo TFT
  tft.setRotation(1);      // Xoay màn hình ngang
  tft.setBrightness(255);  // Độ sáng max

  lv_init();  // Init LVGL
  lv_tick_set_cb(my_tick_get_cb);

  // Tạo display LVGL
  lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

  // Cấp phát buffer
  draw_buf = new uint8_t[BUF_SIZE];

  // Set buffer (render partial)
  lv_display_set_buffers(disp, draw_buf, NULL, BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set callback flush
  lv_display_set_flush_cb(disp, my_disp_flush);
}

//----------------------------------------FUNCTIONS

// Map giá trị analog -> AQI tương ứng
int mapSensor(int pin, int min_aqi, int max_aqi) {
  int val = analogRead(pin);
  return map(val, 0, 4095, min_aqi, max_aqi);
}

// Tính AQI cho PM2.5 theo chuẩn
int getAQI_PM25(int pm25) {
  if (pm25 <= 12) return map(pm25, 0, 12, 0, 50);
  if (pm25 <= 35) return map(pm25, 13, 35, 51, 100);
  if (pm25 <= 55) return map(pm25, 36, 55, 101, 150);
  if (pm25 <= 150) return map(pm25, 56, 150, 151, 200);
  if (pm25 <= 250) return map(pm25, 151, 250, 201, 300);
  return 300;
}

//----------------------------------------WEB SERVER
void initWebServer() {

  // Route trang chính (HTML dashboard)
  server.on("/", []() {
    server.send(200, "text/html", DASHBOARD_HTML);
  });

  // API trả dữ liệu JSON sensor
  server.on("/data", []() {
    JsonDocument doc;

    // Gán toàn bộ dữ liệu vào JSON
    doc["aqi"] = sensorData.aqi;
    doc["pollutant"] = sensorData.pollutant;
    doc["aqi_status"] = sensorData.status;
    doc["weather"] = sensorData.weather;
    doc["pressure"] = sensorData.pressure;
    doc["temp"] = sensorData.temp;
    doc["humi"] = sensorData.humi;
    doc["pm25"] = sensorData.pm25;
    doc["pm10"] = sensorData.pm10;
    doc["co2"] = sensorData.co2;
    doc["o3"] = sensorData.o3;
    doc["co"] = sensorData.co;
    doc["no2"] = sensorData.no2;
    doc["nh3"] = sensorData.nh3;
    doc["voc"] = sensorData.voc;
    doc["fanMode"] = fanMode;

    String json;
    serializeJson(doc, json);

    // Gửi JSON về client
    server.send(200, "application/json", json);
  });
  
  server.on("/control", []() {
    if (server.hasArg("c") && server.hasArg("m")) {
      String ctrl = server.arg("c");
      String mode = server.arg("m");
      if (ctrl == "fan") {
        fanMode = mode;
        Serial.printf("[Control] Fan mode: %s\n", fanMode.c_str());
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing args");
    }
  });

  server.begin();
  Serial.println("Web Server Started at http://192.168.1.4");
}

//----------------------------------------UPDATE SENSOR
void update_Sensor() {

  // Đọc SHT31
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t)) sensorData.temp = t;
  if (!isnan(h)) sensorData.humi = h;

  Serial.printf("Temp: %.1f, Humi: %.0f\n", sensorData.temp, sensorData.humi);

  // Đọc PMS (PM2.5, PM10)
  pms.requestRead();
  if (pms.readUntil(pmsData)) {
    sensorData.pm25 = pmsData.PM_AE_UG_2_5;
    sensorData.pm10 = pmsData.PM_AE_UG_10_0;
    Serial.printf("PM2.5: %d |PM10: %d\n", sensorData.pm25, sensorData.pm10);
  }

  // Đọc các cảm biến khí
  sensorData.co2 = mapSensor(PIN_MQ135, 300, 500);
  sensorData.o3 = mapSensor(PIN_MQ131, 0, 300);
  sensorData.co  = mapSensor(PIN_CJMCU_CO, 0, 150);
  sensorData.no2 = mapSensor(PIN_CJMCU_NO2, 0, 200);
  sensorData.nh3 = mapSensor(PIN_CJMCU_NH3, 0, 300);
  sensorData.voc = mapSensor(PIN_VOC_EXT, 0, 500);

  // Tính AQI từng loại
  int aqi_pm25 = getAQI_PM25(sensorData.pm25);
  int aqi_o3 = map(sensorData.o3, 0, 3000, 0, 300);
  int aqi_no2 = map(sensorData.no2, 0, 200, 0, 200);
  int aqi_nh3 = map(sensorData.nh3, 0, 300, 0, 150);
  int aqi_voc = map(sensorData.voc, 0, 500, 0, 300);
  int aqi_co2 = (sensorData.co2 > 1200) ? 100 : 30;

  // Tìm chất ô nhiễm chính (AQI max)
  int max_aqi = aqi_pm25;
  String main_pol = "PM2.5";

  if (aqi_o3 > max_aqi) {
    max_aqi = aqi_o3;
    main_pol = "OZONE";
  }
  if (aqi_no2 > max_aqi) {
    max_aqi = aqi_no2;
    main_pol = "NO2";
  }
  if (aqi_nh3 > max_aqi) {
    max_aqi = aqi_nh3;
    main_pol = "NH3";
  }
  if (aqi_voc > max_aqi) {
    max_aqi = aqi_voc;
    main_pol = "VOCs";
  }
  if (aqi_co2 > max_aqi) {
    max_aqi = aqi_co2;
    main_pol = "CO2";
  }

  sensorData.aqi = max_aqi;
  sensorData.pollutant = main_pol;

  // Debug log
  Serial.println("--- MEASURE ---");

  // ----------- UPDATE UI -----------
  // (Phần này quyết định màu, icon, trạng thái AQI)

  uint32_t color_hex;
  const lv_img_dsc_t* target_img;
  const char* txt_status;
  const char* txt_advice;

  // Phân loại AQI theo mức độ
  if (max_aqi <= 50) {
    color_hex = 0x3b933f;
    target_img = &img_tron1;
    txt_status = "AIR IS FRESH";
    sensorData.status = "FRESH";
    txt_advice = "Keep your purifier on low and enjoy the day!";
  } else if (max_aqi <= 100) {
    color_hex = 0xe2b808;
    target_img = &img_tron2;
    txt_status = "AIR IS ACCEPTABLE";
    sensorData.status = "ACCEPTABLE";
    txt_advice = "Run purifier indoors for extra comfort.";
  } else if (max_aqi <= 150) {
    color_hex = 0xff773d;
    target_img = &img_tron3;
    txt_status = "SENSITIVE LUNGS MAY NOTICE THE AIR.";
    sensorData.status = "SENSITIVE";
    txt_advice = "Sensitive groups should reduce outdoor activity.";
  } else if (max_aqi <= 200) {
    color_hex = 0xe31919;
    target_img = &img_tron4;
    txt_status = "AIR IS POOR";
    sensorData.status = " POOR";
    txt_advice = "Stay indoors with purifier on high for protection.";
  } else if (max_aqi <= 300) {
    color_hex = 0xab52c5;
    target_img = &img_tron5;
    txt_status = "AIR IS HEAVY";
    sensorData.status = "HEAVY";
    txt_advice = "Limit time outside. Your purifier will guard your indoor air..";
  } else {
    color_hex = 0x800000;
    target_img = &img_tron6;
    txt_status = "AIR IS DANGEROUS";
    sensorData.status = "DANGEROUS";
    txt_advice = "Stay indoors, purifier running. Your health comes first.";
  }

  // Update label nhiệt độ / độ ẩm
  int thap_phan = (int)(sensorData.temp * 10) % 10;
  if (objects.temp) lv_label_set_text_fmt(objects.temp, "%d.%d C", int(sensorData.temp), abs(thap_phan));
  if (objects.humi) lv_label_set_text_fmt(objects.humi, "%d%%", int(sensorData.humi));

  // Update pollutant chính
  if (objects.main_pollutain) lv_label_set_text(objects.main_pollutain, sensorData.pollutant.c_str());

  // Update AQI + màu sắc + animation
  if (objects.aqi) {
    lv_label_set_text_fmt(objects.aqi, "%d", max_aqi);
    lv_obj_set_style_text_color(objects.aqi, lv_color_hex(color_hex), 0);

    if (objects.tron) {
      static const lv_img_dsc_t* anim_imgs[1];
      anim_imgs[0] = target_img;
      lv_animimg_set_src(objects.tron, (const void**)anim_imgs, 1);
      lv_animimg_start(objects.tron);
    }
    if (objects.air_is) {
      lv_label_set_text(objects.air_is, txt_status);
      lv_obj_set_style_text_color(objects.air_is, lv_color_hex(color_hex), 0);
    }
    if (objects.cach_xu_ly) lv_label_set_text(objects.cach_xu_ly, txt_advice);
  }
}

//----------------------------------------FETCH WEATHER
void fetch_Weather() {

  // Nếu chưa có WiFi thì bỏ qua
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  // URL API
  String url = "http://api.openweathermap.org/data/2.5/weather?q="
               + city + "," + countryCode
               + "&units=metric&APPID=" + openWeatherMapApiKey;

  http.begin(url);

  // Nếu request OK
  if (http.GET() == HTTP_CODE_OK) {

    String payload = http.getString();

    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    const char* main = doc["weather"][0]["main"];
    int press = doc["main"]["pressure"];

    // Lưu vào struct
    sensorData.weather = String(main);
    sensorData.pressure = press;

    // Update UI
    if (objects.weather) lv_label_set_text(objects.weather, main);

    Serial.printf("Weather: %s\n", main);
  }

  http.end();
}

//----------------------------------------SETUP
void setup() {

  // UART cho PMS
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.begin(115200);
  delay(500);

  // I2C init
  Wire.begin();

  // Init SHT31
  if (!sht31.begin(0x44) && !sht31.begin(0x45)) Serial.println("SHT31 Error");
  else Serial.println("SHT31 OK!");

  init_Display_System();  // Init màn hình
  ui_init();              // Init UI LVGL
  
  // Khởi tạo WiFi thông qua thư viện wifi_config.h của bạn
  connected = wifi_init(ssid, password, server);

  // Nếu kết nối STA thành công → đăng ký route web chính
  if (connected) {
    initWebServer();
  } else {
    // AP mode → wifi_config.h đã đăng ký route captive portal, chỉ cần begin
    server.begin();
    Serial.println("AP WebServer started.");
  }

  // Init PMS
  pms.passiveMode();
  pms.wakeUp();

  // Lần đọc đầu tiên
  update_Sensor();

  if (WiFi.status() == WL_CONNECTED) fetch_Weather();
}

//----------------------------------------LOOP
void loop() {

  wifi_handleDNS();                            // Xử lý DNS captive portal thông qua wifi_config.h
  server.handleClient();                       // Xử lý request web
  lv_timer_handler();                          // LVGL task

  unsigned long current = millis();

  // Update sensor định kỳ
  if (current - prevMillSensor >= INTERVAL_SENSOR) {
    prevMillSensor = current;
    update_Sensor();
  }

  // Update weather định kỳ
  if (current - prevMillWeather >= INTERVAL_WEATHER) {
    prevMillWeather = current;
    fetch_Weather();
  }

  delay(5);  // Nhẹ CPU
}
