/*
 * esp32_st7735.ino
 * ─────────────────────────────────────────────────────────
 * Debug Monitor cho ElectroShop — hiển thị trên ST7735 128x160
 * 
 * THƯ VIỆN CẦN CÀI (Arduino Library Manager):
 *   - Adafruit ST7735 and ST7789 Library
 *   - Adafruit GFX Library
 *   - ArduinoJson  (by Benoit Blanchon)
 *
 * ─── SƠ ĐỒ NỐI DÂY ───────────────────────────────────────
 *  ST7735 Pin │ ESP32 Pin  │ Ghi chú
 *  ───────────┼────────────┼──────────────────────
 *  VCC        │ 3.3V       │ KHÔNG dùng 5V!
 *  GND        │ GND        │
 *  CS  (CS)   │ GPIO 5     │ Chip Select
 *  RESET(RST) │ GPIO 4     │ Reset
 *  DC  (A0)   │ GPIO 2     │ Data/Command
 *  SDA (MOSI) │ GPIO 23    │ Data (SPI MOSI)
 *  SCK (CLK)  │ GPIO 18    │ Clock (SPI CLK)
 *  LED (BL)   │ 3.3V       │ Đèn nền (hoặc GPIO để điều chỉnh)
 * ─────────────────────────────────────────────────────────
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ArduinoJson.h>
#include <SPI.h>

// ── Cấu hình WiFi — SỬA CHỖ NÀY ─────────────────────────
const char* WIFI_SSID = "TEN_WIFI_CUA_BAN";
const char* WIFI_PASS = "MAT_KHAU_WIFI";
// ─────────────────────────────────────────────────────────

// ── Chân ST7735 ──────────────────────────────────────────
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
// MOSI = 23, SCK = 18 (SPI mặc định của ESP32)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// ── Màu sắc ──────────────────────────────────────────────
#define C_BG       ST77XX_BLACK
#define C_TITLE    0x07FF   // Cyan
#define C_LABEL    0xFFE0   // Yellow
#define C_OK       ST77XX_GREEN
#define C_ERR      ST77XX_RED
#define C_PRODUCT  0xF81F   // Magenta
#define C_LOG      0xBDF7   // Light gray
#define C_DIM      0x4208   // Dark gray
#define C_WHITE    ST77XX_WHITE
#define C_ORANGE   0xFD20

// ── Trạng thái ───────────────────────────────────────────
#define MAX_LOGS 5
String  g_flask   = "...";
String  g_pb      = "...";
String  g_tunnel  = "...";
String  g_product = "";
String  g_logs[MAX_LOGS];
uint8_t g_logCount = 0;
bool    g_dirty   = true;   // Cần vẽ lại màn hình?
String  g_myIP    = "";

// ── Vẽ màn hình ──────────────────────────────────────────
void drawStatusDot(int x, int y, String val) {
    uint16_t col = (val == "OK") ? C_OK : C_ERR;
    tft.fillCircle(x, y, 3, col);
}

void drawHLine(int y, uint16_t col = C_DIM) {
    tft.drawFastHLine(0, y, 128, col);
}

void drawScreen() {
    tft.fillScreen(C_BG);

    // ── HEADER ──────────────────────────────────── y=0..12
    tft.fillRect(0, 0, 128, 13, 0x0010); // Nền xanh đậm
    tft.setTextColor(C_TITLE);
    tft.setTextSize(1);
    tft.setCursor(4, 3);
    tft.print("ElectroShop  Debug");

    // ── TRẠNG THÁI DỊCH VỤ ─────────────────────── y=16..40
    // Flask
    tft.setTextColor(C_LABEL);
    tft.setCursor(4, 17);
    tft.print("Flask  ");
    drawStatusDot(55, 21, g_flask);
    tft.setTextColor(g_flask == "OK" ? C_OK : C_ERR);
    tft.setCursor(62, 17);
    tft.print(g_flask);

    // PocketBase
    tft.setTextColor(C_LABEL);
    tft.setCursor(4, 29);
    tft.print("PocketB");
    drawStatusDot(55, 33, g_pb);
    tft.setTextColor(g_pb == "OK" ? C_OK : C_ERR);
    tft.setCursor(62, 29);
    tft.print(g_pb);

    // Tunnel
    tft.setTextColor(C_LABEL);
    tft.setCursor(4, 41);
    tft.print("Tunnel ");
    tft.setTextColor(C_WHITE);
    String t = g_tunnel.length() > 14 ? g_tunnel.substring(0, 14) : g_tunnel;
    tft.setCursor(46, 41);
    tft.print(t);

    drawHLine(53, C_DIM);

    // ── SẢN PHẨM ĐANG XỬ LÝ ────────────────────── y=55..80
    tft.setTextColor(C_ORANGE);
    tft.setCursor(4, 56);
    tft.print("> AI dang xu ly:");

    tft.setTextColor(C_PRODUCT);
    String prod = g_product.isEmpty() ? "(cho yeu cau...)" : g_product;
    // Xuống dòng thủ công nếu quá dài (21 ký tự / dòng)
    if (prod.length() <= 21) {
        tft.setCursor(4, 66);
        tft.print(prod);
    } else {
        tft.setCursor(4, 66);
        tft.print(prod.substring(0, 21));
        tft.setCursor(4, 76);
        String line2 = prod.substring(21, 42);
        tft.print(line2);
    }

    drawHLine(88, C_DIM);

    // ── 5 DÒNG LOG GẦN NHẤT ─────────────────────── y=90..155
    tft.setTextColor(C_LABEL);
    tft.setCursor(4, 91);
    tft.print("Logs:");

    for (int i = 0; i < MAX_LOGS; i++) {
        int y = 101 + i * 12;
        if (y > 155) break;
        tft.setTextColor(i == 0 ? C_WHITE : C_LOG);
        tft.setCursor(4, y);
        String line = g_logs[i];
        if (line.isEmpty()) line = "---";
        if (line.length() > 21) line = line.substring(0, 21);
        tft.print(line);
    }

    g_dirty = false;
}

// ── Thêm log mới ─────────────────────────────────────────
void addLog(String msg) {
    for (int i = MAX_LOGS - 1; i > 0; i--) g_logs[i] = g_logs[i - 1];
    g_logs[0] = msg;
    g_dirty = true;
}

// ── Màn hình đang kết nối WiFi ────────────────────────────
void drawConnecting(int dots) {
    tft.fillScreen(C_BG);
    tft.setTextColor(C_TITLE);
    tft.setTextSize(1);
    tft.setCursor(10, 55);
    tft.print("Dang ket noi WiFi");
    tft.setCursor(10, 68);
    tft.setTextColor(C_LABEL);
    for (int i = 0; i < dots % 6; i++) tft.print(".");
}

// ── HTTP: POST /update ────────────────────────────────────
void handleUpdate() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No body"); return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "text/plain", "JSON error"); return;
    }

    if (doc.containsKey("flask"))   { String v = doc["flask"];   if (v != g_flask)   { g_flask = v;   g_dirty = true; } }
    if (doc.containsKey("pb"))      { String v = doc["pb"];      if (v != g_pb)      { g_pb = v;      g_dirty = true; } }
    if (doc.containsKey("tunnel"))  { String v = doc["tunnel"];  if (v != g_tunnel)  { g_tunnel = v;  g_dirty = true; } }
    if (doc.containsKey("product")) { String v = doc["product"]; if (v != g_product) { g_product = v; g_dirty = true; } }
    if (doc.containsKey("log"))     { addLog(doc["log"].as<String>()); }
    if (doc.containsKey("logs") && doc["logs"].is<JsonArray>()) {
        JsonArray arr = doc["logs"].as<JsonArray>();
        int i = 0;
        for (JsonVariant v : arr) { if (i < MAX_LOGS) g_logs[i++] = v.as<String>(); }
        g_dirty = true;
    }

    server.send(200, "application/json", "{\"ok\":true}");
}

// ── HTTP: GET /status ─────────────────────────────────────
void handleStatus() {
    String json = "{\"ok\":true,\"display\":\"online\",\"ip\":\"" + g_myIP + "\"}";
    server.send(200, "application/json", json);
}

// ── Serial: đọc JSON từ Pi ────────────────────────────────
void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line[0] != '{') return;

    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, line)) return;

    if (doc.containsKey("flask"))   g_flask   = doc["flask"].as<String>();
    if (doc.containsKey("pb"))      g_pb      = doc["pb"].as<String>();
    if (doc.containsKey("tunnel"))  g_tunnel  = doc["tunnel"].as<String>();
    if (doc.containsKey("product")) { g_product = doc["product"].as<String>(); addLog("> " + g_product.substring(0, 16)); }
    if (doc.containsKey("log"))     addLog(doc["log"].as<String>());
    g_dirty = true;
}

// ── SETUP ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Khởi động ST7735
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);   // 0 = dọc (portrait), 128x160
    tft.fillScreen(C_BG);
    tft.setTextWrap(false);

    // Kết nối WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        drawConnecting(++dots);
        delay(500);
    }

    g_myIP = WiFi.localIP().toString();
    Serial.println("[ESP32] WiFi OK, IP: " + g_myIP);

    // Màn hình chào
    tft.fillScreen(C_BG);
    tft.setTextColor(C_OK);
    tft.setTextSize(1);
    tft.setCursor(10, 52);
    tft.print("WiFi ket noi OK!");
    tft.setTextColor(C_WHITE);
    tft.setCursor(10, 65);
    tft.print("IP: " + g_myIP);
    tft.setTextColor(C_DIM);
    tft.setCursor(10, 80);
    tft.print("Cho du lieu tu Pi...");
    delay(2500);

    // HTTP server
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/status", HTTP_GET,  handleStatus);
    server.begin();
    Serial.println("[ESP32] HTTP Server started");

    addLog("San sang | " + g_myIP);
    drawScreen();
}

// ── LOOP ──────────────────────────────────────────────────
void loop() {
    server.handleClient();
    handleSerial();
    if (g_dirty) drawScreen();
    delay(50);
}
