/*
 * ==============================================================================
 * ESP32 NEXUS CORE - DUAL FIRMWARE (BLE BLUETOOTH + HTTP REST & mDNS)
 * ==============================================================================
 * Dự án: ESP32 PWA Dashboard
 * Chức năng:
 *  - Bluetooth Low Energy (BLE GATT Server): Dịch vụ truyền nhận Telemetry & Điều khiển
 *    Tên thiết bị BLE: "ESP32_NEXUS"
 *    Service UUID:        4fa0c101-0001-4000-8000-000000000000
 *    Characteristic UUID: 4fa0c101-0002-4000-8000-000000000000
 *  - WiFi Dual Mode (AP riêng 192.168.4.1 + STA kết nối Router)
 *  - mDNS: http://esp32-nexus.local
 *  - Tương thích 100% với Web Bluetooth API & PWA GitHub Pages
 * ==============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ================== CẤU HÌNH BLE UUID ==================
#define BLE_SERVICE_UUID        "4fa0c101-0001-4000-8000-000000000000"
#define BLE_CHARACTERISTIC_UUID "4fa0c101-0002-4000-8000-000000000000"

BLECharacteristic *pBleChar = nullptr;
bool bleConnected = false;
bool oldBleConnected = false;

// ================== CẤU HÌNH THÔNG TIN WIFI ==================
const char* WIFI_SSID     = "1/11 Tret";      
const char* WIFI_PASSWORD = "88889999@";  
const char* MDNS_HOST     = "esp32-nexus";

WebServer server(80);

// ================== BIẾN TRẠNG THÁI HỆ THỐNG ==================
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_INTERVAL = 1000; // Cập nhật dữ liệu mỗi 1 giây (1000ms)

float sensorTemp   = 28.5; // °C
float sensorHum    = 65.0; // % RH
float sensorVolt   = 4.15; // Volts
int   sensorRssi   = -55;  // dBm

bool relayStates[4] = {false, false, false, false};
const int RELAY_PINS[4] = {25, 26, 27, 14};

// ================== HÀM HỖ TRỢ CORS ==================
void applyCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
}

// ================== HÀM CẬP NHẬT DỮ LIỆU TELEMETRY ==================
void updateTelemetryValues() {
  sensorTemp = 28.0 + (random(0, 80) / 10.0);   
  sensorHum  = 55.0 + (random(0, 300) / 10.0);  
  sensorVolt = 3.80 + (random(0, 40) / 100.0);  
  
  if (WiFi.status() == WL_CONNECTED) {
    sensorRssi = WiFi.RSSI();
  } else if (bleConnected) {
    sensorRssi = -50 + random(-10, 5); // Ước lượng RSSI BLE
  } else {
    sensorRssi = -60;
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

// ================== XỬ LÝ LỆNH ĐIỀU KHIỂN CHUNG (BLE & HTTP) ==================
void handleCommandString(String cmd) {
  cmd.trim();
  Serial.println("[CMD Nhận được]: " + cmd);

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

  if (cmd.startsWith("PWM:")) {
    int pwmVal = cmd.substring(4).toInt();
    Serial.printf("-> PWM: %d\n", pwmVal);
  }
}

// ================== CALLBACKS CHO BLUETOOTH BLE ==================
class BleServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    Serial.println("\n[BLE] >>> Thiết bị Web PWA đã kết nối Bluetooth! <<<");
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    Serial.println("\n[BLE] >>> Thiết bị Web PWA đã ngắt kết nối. <<<");
  }
};

class BleCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      handleCommandString(rxValue);
    }
  }
};

// ================== CÁC ENDPOINT REST API HTTP ==================
void handleApiData() {
  applyCorsHeaders();
  server.send(200, "application/json", generateTelemetryJson());
}

void handleOptions() {
  applyCorsHeaders();
  server.send(204);
}

void handleApiControl() {
  applyCorsHeaders();
  if (server.hasArg("cmd")) {
    handleCommandString(server.arg("cmd"));
  }
  String response = "{\"status\":\"ok\",\"relays\":[" + 
                    String(relayStates[0] ? "true" : "false") + "," +
                    String(relayStates[1] ? "true" : "false") + "," +
                    String(relayStates[2] ? "true" : "false") + "," +
                    String(relayStates[3] ? "true" : "false") + "]}";
  server.send(200, "application/json", response);
}

void handleRoot() {
  applyCorsHeaders();
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Nexus Core</title></head>";
  html += "<body style='background:#0f172a;color:#38bdf8;font-family:sans-serif;text-align:center;padding:50px;'>";
  html += "<h1>ESP32 NEXUS CORE (BLE + WIFI)</h1>";
  html += "<p>Bluetooth BLE: <b>ESP32_NEXUS</b> (Sẵn sàng kết nối)</p>";
  html += "<p>Tên miền mDNS: <a href='http://" + String(MDNS_HOST) + ".local/api/data' style='color:#4ade80;'>http://" + String(MDNS_HOST) + ".local/api/data</a></p>";
  html += "<p>IP nội mạng: " + WiFi.localIP().toString() + "</p>";
  html += "<p>IP Access Point: " + WiFi.softAPIP().toString() + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

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
  Serial.println("  ESP32 NEXUS CORE - DUAL BLE & WIFI   ");
  Serial.println("========================================");

  // 1. Cấu hình chân Rơ-le
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // 2. KHỞI TẠO BLUETOOTH LOW ENERGY (BLE)
  Serial.println("[BLE] Đang khởi tạo Bluetooth BLE Server...");
  BLEDevice::init("ESP32_NEXUS");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BleServerCallbacks());

  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);
  pBleChar = pService->createCharacteristic(
                BLE_CHARACTERISTIC_UUID,
                BLECharacteristic::PROPERTY_READ   |
                BLECharacteristic::PROPERTY_WRITE  |
                BLECharacteristic::PROPERTY_NOTIFY
              );
  pBleChar->addDescriptor(new BLE2902());
  pBleChar->setCallbacks(new BleCharCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] >>> ESP32_NEXUS đã phát quảng bá Bluetooth thành công! <<<");

  // 3. KHỞI TẠO WIFI DUAL MODE (AP + STA)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_NEXUS", "12345678");
  Serial.println("[WiFi AP] Đang phát mạng: ESP32_NEXUS (IP: 192.168.4.1)");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối vào WiFi nhà: ");
  Serial.println(WIFI_SSID);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
    delay(400);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi STA] Đã kết nối WiFi!");
    Serial.print("[WiFi STA] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi STA] Tiếp tục với mạng riêng và BLE.");
  }

  // 4. KHỞI TẠO mDNS & WEBSERVER
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] Sẵn sàng: http://esp32-nexus.local");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/data", HTTP_OPTIONS, handleOptions);
  server.on("/api/control", HTTP_GET, handleApiControl);
  server.on("/api/control", HTTP_POST, handleApiControl);
  server.on("/api/control", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[HTTP Server] Sẵn sàng tại cổng 80.");
}

// ================== LOOP CHÍNH ==================
void loop() {
  // Lắng nghe HTTP request
  server.handleClient();

  // Tự động phát quảng bá lại nếu ngắt kết nối BLE
  if (!bleConnected && oldBleConnected) {
    delay(500); 
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Đã bật lại phát quảng bá (Advertising) chờ kết nối lại...");
    oldBleConnected = bleConnected;
  }
  if (bleConnected && !oldBleConnected) {
    oldBleConnected = bleConnected;
  }

  // Cập nhật và gửi Telemetry mỗi 1 giây
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= SENSOR_INTERVAL) {
    lastSensorUpdate = currentMillis;
    updateTelemetryValues();

    // Nếu có thiết bị Web PWA đang kết nối BLE -> Bắn dữ liệu Notify thời gian thực
    if (bleConnected && pBleChar != nullptr) {
      String jsonStr = generateTelemetryJson();
      pBleChar->setValue(jsonStr.c_str());
      pBleChar->notify();
    }
  }
}
