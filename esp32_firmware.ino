/*
 * ==============================================================================
 * ESP32 NEXUS CORE - FIRMWARE HTTP REST & mDNS TELEMETRY SERVER
 * ==============================================================================
 * Dự án: ESP32 PWA Dashboard
 * Chức năng:
 *  - Kết nối WiFi gia đình (STA mode)
 *  - Đăng ký tên miền mDNS: http://esp32-nexus.local
 *  - Endpoint /api/data: Cung cấp dữ liệu JSON cảm biến mỗi 1 giây (CORS enabled)
 *  - Endpoint /api/control: Nhận lệnh điều khiển Rơ-le, PWM từ PWA Dashboard
 *  - Tương thích 100% với PWA Hosted trên GitHub Pages
 * ==============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ================== CẤU HÌNH THÔNG TIN WIFI ==================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";      // Thay bằng tên WiFi nhà bạn
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Thay bằng mật khẩu WiFi

// Tên miền mDNS (sẽ truy cập qua http://esp32-nexus.local)
const char* MDNS_HOST     = "esp32-nexus";

// Khởi tạo WebServer tại cổng tiêu chuẩn 80
WebServer server(80);

// ================== BIẾN TRẠNG THÁI HỆ THỐNG ==================
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_INTERVAL = 1000; // Cập nhật dữ liệu mỗi 1 giây (1000ms)

// Các thông số Telemetry giả lập
float sensorTemp   = 28.5; // °C
float sensorHum    = 65.0; // % RH
float sensorVolt   = 4.15; // Volts
int   sensorRssi   = -55;  // dBm

// Trạng thái 4 Rơ-le (Relay 1..4)
bool relayStates[4] = {false, false, false, false};

// Chân GPIO Rơ-le vật lý (nếu có kết nối phần cứng thật)
const int RELAY_PINS[4] = {25, 26, 27, 14};

// ================== HÀM HỖ TRỢ CORS ==================
// Đảm bảo trình duyệt từ GitHub Pages không bị chặn do Cross-Origin
void applyCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
}

// ================== HÀM CẬP NHẬT DỮ LIỆU TELEMETRY ==================
void updateTelemetryValues() {
  // Sinh giá trị ngẫu nhiên dao động thực tế mỗi 1 giây
  sensorTemp = 28.0 + (random(0, 80) / 10.0);   // Dao động từ 28.0 - 36.0 °C
  sensorHum  = 55.0 + (random(0, 300) / 10.0);  // Dao động từ 55.0 - 85.0 %
  sensorVolt = 3.80 + (random(0, 40) / 100.0);  // Dao động từ 3.80 - 4.20 V
  
  if (WiFi.status() == WL_CONNECTED) {
    sensorRssi = WiFi.RSSI();
  } else {
    sensorRssi = -55 + random(-10, 10);
  }
}

// Tạo chuỗi JSON Telemetry
String generateTelemetryJson() {
  String json = "{";
  json += "\"temp\":" + String(sensorTemp, 1) + ",";
  json += "\"hum\":" + String(sensorHum, 1) + ",";
  json += "\"volt\":" + String(sensorVolt, 2) + ",";
  json += "\"rssi\":" + String(sensorRssi) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"relays\":[" + 
          String(relayStates[0] ? "true" : "false") + "," +
          String(relayStates[1] ? "true" : "false") + "," +
          String(relayStates[2] ? "true" : "false") + "," +
          String(relayStates[3] ? "true" : "false") + "]";
  json += "}";
  return json;
}

// ================== CÁC ENDPOINT REST API ==================

// GET /api/data: Cung cấp JSON dữ liệu cảm biến
void handleApiData() {
  applyCorsHeaders();
  server.send(200, "application/json", generateTelemetryJson());
}

// OPTIONS /api/data & /api/control: Xử lý Preflight CORS request
void handleOptions() {
  applyCorsHeaders();
  server.send(204); // No Content
}

// GET hoặc POST /api/control?cmd=...: Điều khiển Rơ-le và thiết bị
void handleApiControl() {
  applyCorsHeaders();
  
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    Serial.println("[HTTP CMD Nhận được]: " + cmd);

    // Xử lý lệnh Rơ-le (VD: RELAY1:ON, RELAY2:OFF)
    for (int i = 1; i <= 4; i++) {
      String onCmd  = "RELAY" + String(i) + ":ON";
      String offCmd = "RELAY" + String(i) + ":OFF";
      
      if (cmd == onCmd) {
        relayStates[i - 1] = true;
        digitalWrite(RELAY_PINS[i - 1], HIGH);
        Serial.printf("-> Rơ-le %d: BẬT\n", i);
      } else if (cmd == offCmd) {
        relayStates[i - 1] = false;
        digitalWrite(RELAY_PINS[i - 1], LOW);
        Serial.printf("-> Rơ-le %d: TẮT\n", i);
      }
    }
  }

  String response = "{\"status\":\"ok\",\"relays\":[" + 
                    String(relayStates[0] ? "true" : "false") + "," +
                    String(relayStates[1] ? "true" : "false") + "," +
                    String(relayStates[2] ? "true" : "false") + "," +
                    String(relayStates[3] ? "true" : "false") + "]}";
  server.send(200, "application/json", response);
}

// Trang chủ HTTP /
void handleRoot() {
  applyCorsHeaders();
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Nexus Core</title></head>";
  html += "<body style='background:#0f172a;color:#38bdf8;font-family:sans-serif;text-align:center;padding:50px;'>";
  html += "<h1>ESP32 NEXUS CORE ONLINE</h1>";
  html += "<p>Tên miền mDNS: <a href='http://" + String(MDNS_HOST) + ".local/api/data' style='color:#4ade80;'>http://" + String(MDNS_HOST) + ".local/api/data</a></p>";
  html += "<p>IP nội mạng: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Xử lý 404 Not Found kèm CORS
void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
  } else {
    applyCorsHeaders();
    server.send(404, "application/json", "{\"error\":\"Not Found\"}");
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("   ESP32 NEXUS CORE - KHỞI ĐỘNG HỆ THỐNG   ");
  Serial.println("========================================");

  // Cấu hình chân Rơ-le
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // Kết nối WiFi gia đình (STA Mode)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối vào WiFi: ");
  Serial.println(WIFI_SSID);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Đã kết nối thành công!");
    Serial.print("[WiFi] Địa chỉ IP nội mạng: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi CẢNH BÁO] Không thể kết nối WiFi chính! Kích hoạt chế độ Access Point dự phòng...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_NEXUS_CORE", "12345678");
    Serial.print("[WiFi AP] IP Access Point: ");
    Serial.println(WiFi.softAPIP());
  }

  // Đăng ký tên miền mDNS (esp32-nexus.local)
  if (MDNS.begin(MDNS_HOST)) {
    Serial.print("[mDNS] Đã đăng ký thành công! Truy cập PWA qua: http://");
    Serial.print(MDNS_HOST);
    Serial.println(".local");
    // Thêm dịch vụ HTTP vào mDNS
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[mDNS LỖI] Không thể khởi động mDNS responder!");
  }

  // Đăng ký các Route API cho WebServer
  server.on("/", HTTP_GET, handleRoot);
  
  // Endpoint /api/data
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/data", HTTP_OPTIONS, handleOptions);
  
  // Endpoint /api/control
  server.on("/api/control", HTTP_GET, handleApiControl);
  server.on("/api/control", HTTP_POST, handleApiControl);
  server.on("/api/control", HTTP_OPTIONS, handleOptions);

  server.onNotFound(handleNotFound);

  // Kích hoạt Server
  server.begin();
  Serial.println("[HTTP Server] Máy chủ đã sẵn sàng nhận kết nối tại cổng 80.");
}

// ================== LOOP CHÍNH ==================
void loop() {
  // Lắng nghe các HTTP request đến
  server.handleClient();

  // Cập nhật giá trị cảm biến mỗi 1 giây (1000ms)
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= SENSOR_INTERVAL) {
    lastSensorUpdate = currentMillis;
    updateTelemetryValues();
  }
}
