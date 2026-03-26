/*
 * cyd_monitor.ino — ElectroShop Dashboard cho CYD ESP32-2432S035
 * ═══════════════════════════════════════════════════════════════
 * Màn hình 3.5" ILI9488 (480×320, landscape)  |  Touch XPT2046
 * Nhận dữ liệu từ Pi 5 qua USB Serial (CH340C) @ 115200 baud
 *
 * THƯ VIỆN (Arduino Library Manager):
 *   - TFT_eSPI         (Bodmer)
 *   - ArduinoJson      (Benoit Blanchon)  v6+
 *
 * SAO CHÉP User_Setup.h vào thư mục TFT_eSPI trong libraries trước khi nạp!
 *
 * 4 TRANG cảm ứng:
 *   [HOME]  Tunnel + Flask + PocketBase + đơn mới nhất
 *   [ORDERS] 3 đơn hàng gần nhất
 *   [NEWS]   3 tin tức mới nhất
 *   [STATS]  Thống kê web + tài nguyên Pi 5
 */

#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// ── Màu sắc ──────────────────────────────────────────────
#define C_BG       0x0000
#define C_HEADER   0x1082
#define C_GRAY     0x8410
#define C_LGRAY    0xC618
#define C_WHITE    0xFFFF
#define C_CYAN     0x07FF
#define C_GREEN    0x07E0
#define C_DKGREEN  0x0320
#define C_RED      0xF800
#define C_DKRED    0x6000
#define C_YELLOW   0xFFE0
#define C_ORANGE   0xFD20
#define C_MAGENTA  0xF81F
#define C_BLUE     0x001F
#define C_DKBLUE   0x000F
#define C_PURPLE   0x780F
#define C_TEAL     0x0410

// ── Layout ───────────────────────────────────────────────
#define SCREEN_W  480
#define SCREEN_H  320
#define HEADER_H   44
#define NAV_H      46
#define CONTENT_Y (HEADER_H + 1)
#define CONTENT_H (SCREEN_H - HEADER_H - NAV_H - 2)
#define NAV_Y     (SCREEN_H - NAV_H)

// ── Trang ────────────────────────────────────────────────
#define PAGE_HOME   0
#define PAGE_ORDERS 1
#define PAGE_NEWS   2
#define PAGE_STATS  3
#define NUM_PAGES   4

// ── Touch calibration (chạy sketch calibration nếu lệch) ─
uint16_t calData[5] = { 275, 3620, 264, 3532, 3 };  // swapXY + invertX

// ── Dữ liệu nhận từ Pi ───────────────────────────────────
struct Order {
  String name, price, buyer, age;
};
struct NewsItem {
  String title, cat, age;
};

String g_time        = "--:--";
String g_date        = "--/--";
String g_tunnel      = "...";
String g_tunnel_url  = "";
String g_flask       = "...";
String g_pb          = "...";
int    g_prod_total  = 0;
int    g_orders_total= 0;
int    g_news_total  = 0;
// Pi 5 system
float  g_cpu_temp    = 0;
int    g_ram_pct     = 0;
int    g_disk_pct    = 0;
String g_pi_ip       = "";
int    g_cpu_pct     = 0;

Order    g_orders[3];
NewsItem g_news[3];
uint8_t  g_order_count = 0;
uint8_t  g_news_count  = 0;

// ── Trạng thái UI ─────────────────────────────────────────
int           currentPage = PAGE_HOME;
bool          needRedraw  = true;
unsigned long lastHeaderRedraw = 0;
unsigned long bootTime    = 0;
String        serialBuffer = "";

// ═══════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════
void fillBar(int x, int y, int w, int h, uint16_t c) {
  tft.fillRect(x, y, w, h, c);
}

// Vẽ thanh tiến trình ngang
void drawBar(int x, int y, int w, int h, int pct, uint16_t fg, uint16_t bg) {
  tft.drawRect(x, y, w, h, C_GRAY);
  fillBar(x+1, y+1, w-2, h-2, bg);
  int filled = ((w-2) * pct) / 100;
  if (filled > 0) fillBar(x+1, y+1, filled, h-2, fg);
}

// Màu theo phần trăm (xanh → vàng → đỏ)
uint16_t pctColor(int pct) {
  if (pct < 60) return C_GREEN;
  if (pct < 80) return C_YELLOW;
  return C_RED;
}

// ═══════════════════════════════════════════════════════════
//  HEADER (cập nhật mỗi giây)
// ═══════════════════════════════════════════════════════════
// Chỉ vẽ lại vùng giờ/ngày — không xóa cả header (tránh nhấp nháy)
void drawHeaderClock() {
  tft.setTextColor(C_YELLOW, C_HEADER);
  tft.setTextSize(2);
  tft.setCursor(SCREEN_W - 108, 8);
  tft.print(g_time);
  tft.setTextSize(1);
  tft.setTextColor(C_LGRAY, C_HEADER);
  tft.setCursor(SCREEN_W - 55, 14);
  tft.print(g_date);
}

void drawHeader() {
  fillBar(0, 0, SCREEN_W, HEADER_H, C_HEADER);

  // Logo
  tft.setTextColor(C_CYAN, C_HEADER);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("ELECTROSHOP");

  // Tunnel dot
  bool ok = (g_tunnel == "ON");
  tft.fillCircle(215, HEADER_H/2, 7, ok ? C_GREEN : C_RED);
  tft.setTextSize(1);
  tft.setTextColor(ok ? C_GREEN : C_RED, C_HEADER);
  tft.setCursor(226, HEADER_H/2 - 4);
  tft.print(ok ? "LIVE" : "DOWN");

  // Giờ + ngày
  tft.setTextColor(C_YELLOW, C_HEADER);
  tft.setTextSize(2);
  tft.setCursor(SCREEN_W - 108, 8);
  tft.print(g_time);
  tft.setTextSize(1);
  tft.setTextColor(C_LGRAY, C_HEADER);
  tft.setCursor(SCREEN_W - 55, 14);
  tft.print(g_date);

  // Dải màu dưới header
  tft.drawFastHLine(0, HEADER_H,   SCREEN_W, C_GRAY);
  tft.drawFastHLine(0, HEADER_H+1, SCREEN_W, C_CYAN);
}

// ═══════════════════════════════════════════════════════════
//  NAV BAR
// ═══════════════════════════════════════════════════════════
void drawNavBar() {
  const char* names[4] = { "HOME", "ORDERS", "NEWS", "STATS" };
  const uint16_t accents[4] = { C_CYAN, C_ORANGE, C_GREEN, C_MAGENTA };

  fillBar(0, NAV_Y, SCREEN_W, NAV_H, 0x0821);
  tft.drawFastHLine(0, NAV_Y, SCREEN_W, C_GRAY);

  int bw = SCREEN_W / NUM_PAGES;
  for (int i = 0; i < NUM_PAGES; i++) {
    int bx = i * bw;
    bool active = (i == currentPage);
    uint16_t bg = active ? 0x1062 : 0x0821;

    fillBar(bx+1, NAV_Y+1, bw-2, NAV_H-2, bg);

    // Accent bar trên top khi active
    if (active) tft.drawFastHLine(bx, NAV_Y, bw, accents[i]);

    tft.setTextColor(active ? accents[i] : C_GRAY, bg);
    tft.setTextSize(active ? 2 : 1);
    int fw = strlen(names[i]) * (active ? 12 : 6);
    tft.setCursor(bx + (bw - fw)/2, NAV_Y + (active ? 14 : 18));
    tft.print(names[i]);

    if (i > 0) tft.drawFastVLine(bx, NAV_Y+4, NAV_H-8, 0x2104);
  }
}

// ═══════════════════════════════════════════════════════════
//  PAGE 0: HOME
// ═══════════════════════════════════════════════════════════
void drawPageHome() {
  int x = 8, y = CONTENT_Y + 6;

  // ── Tunnel status card ──
  bool tunnelOk = (g_tunnel == "ON");
  uint16_t tbg = tunnelOk ? 0x0320 : 0x5000;
  uint16_t tfg = tunnelOk ? C_GREEN : C_RED;

  fillBar(x, y, SCREEN_W - 16, 54, tbg);
  tft.drawRect(x, y, SCREEN_W - 16, 54, tfg);

  tft.setTextColor(tfg, tbg);
  tft.setTextSize(2);
  tft.setCursor(x+10, y+6);
  tft.print("TUNNEL ");
  tft.print(tunnelOk ? "ONLINE  " : "OFFLINE ");
  tft.fillCircle(x + SCREEN_W - 36, y+27, 10, tfg);

  tft.setTextSize(1);
  tft.setTextColor(C_LGRAY, tbg);
  String urlShow = g_tunnel_url.length() > 0 ? g_tunnel_url : "chua co tunnel URL";
  if (urlShow.length() > 52) urlShow = urlShow.substring(0, 52);
  tft.setCursor(x+10, y+36);
  tft.print(urlShow);

  y += 62;

  // ── Service status row ──
  struct { const char* lbl; String* val; uint16_t color; } svcs[3] = {
    { "Flask :5001",  &g_flask, C_CYAN   },
    { "PocketBase",   &g_pb,    C_GREEN  },
    { "Vercel",       nullptr,  C_PURPLE },
  };
  int sw = (SCREEN_W - 16) / 3 - 4;
  for (int i = 0; i < 3; i++) {
    int sx = x + i*(sw+6);
    String val = (i < 2) ? *svcs[i].val : "LIVE";
    bool svcOk = (val == "ON" || val == "LIVE");
    uint16_t sbg = svcOk ? 0x0280 : 0x4000;
    fillBar(sx, y, sw, 36, sbg);
    tft.drawRect(sx, y, sw, 36, svcs[i].color);
    tft.setTextColor(svcs[i].color, sbg);
    tft.setTextSize(1);
    tft.setCursor(sx+6, y+4);
    tft.print(svcs[i].lbl);
    tft.setTextSize(2);
    tft.setCursor(sx+6, y+16);
    tft.print(val);
  }

  y += 44;

  // ── Đơn hàng mới nhất ──
  tft.drawFastHLine(x, y, SCREEN_W-16, 0x2104);
  y += 6;
  tft.setTextColor(C_ORANGE, C_BG);
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.print("DON HANG MOI NHAT:");
  y += 14;

  if (g_order_count > 0) {
    tft.setTextColor(C_WHITE, C_BG);
    tft.setTextSize(1);
    String nm = g_orders[0].name;
    if (nm.length() > 34) nm = nm.substring(0, 34) + "..";
    tft.setCursor(x, y);
    tft.print(nm);
    tft.setTextColor(C_YELLOW, C_BG);
    tft.setCursor(x + 270, y);
    tft.print(g_orders[0].price + "d");
    y += 14;
    tft.setTextColor(C_GRAY, C_BG);
    tft.setCursor(x, y);
    String inf = "KH: " + g_orders[0].buyer + "   " + g_orders[0].age + " truoc";
    if (inf.length() > 58) inf = inf.substring(0, 58);
    tft.print(inf);
  } else {
    tft.setTextColor(C_GRAY, C_BG);
    tft.setCursor(x, y);
    tft.print("Chua co don hang nao...");
  }
}

// ═══════════════════════════════════════════════════════════
//  PAGE 1: ORDERS
// ═══════════════════════════════════════════════════════════
void drawPageOrders() {
  int x = 6, y = CONTENT_Y + 6;

  tft.setTextColor(C_ORANGE, C_BG);
  tft.setTextSize(2);
  tft.setCursor(x+2, y);
  tft.print("DON HANG GAN DAY");
  y += 26;
  tft.drawFastHLine(0, y, SCREEN_W, C_ORANGE);
  y += 6;

  if (g_order_count == 0) {
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextSize(1);
    tft.setCursor(x, y+30);
    tft.print("Chua co don hang nao.");
    return;
  }

  int cardH = (CONTENT_H - 40) / 3;
  for (int i = 0; i < g_order_count && i < 3; i++) {
    int cy = y + i*(cardH + 3);
    uint16_t cbg = (i % 2 == 0) ? 0x0C41 : 0x0821;
    fillBar(x, cy, SCREEN_W-12, cardH, cbg);
    tft.drawRect(x, cy, SCREEN_W-12, cardH, C_ORANGE);

    // Badge số thứ tự
    fillBar(x+2, cy+2, 22, cardH-4, C_ORANGE);
    tft.setTextColor(C_BG, C_ORANGE);
    tft.setTextSize(2);
    tft.setCursor(x+5, cy + cardH/2 - 8);
    tft.print(i+1);

    // Tên sản phẩm
    tft.setTextColor(C_WHITE, cbg);
    tft.setTextSize(1);
    String nm = g_orders[i].name;
    if (nm.length() > 40) nm = nm.substring(0, 40) + "..";
    tft.setCursor(x+30, cy+4);
    tft.print(nm);

    // Giá
    tft.setTextColor(C_YELLOW, cbg);
    tft.setTextSize(2);
    String pr = g_orders[i].price;
    if (pr.length() > 14) pr = pr.substring(0, 14);
    tft.setCursor(x+30, cy+18);
    tft.print(pr + "d");

    // Thông tin phụ
    tft.setTextColor(C_LGRAY, cbg);
    tft.setTextSize(1);
    tft.setCursor(x+30, cy+cardH-14);
    String inf = "KH: " + g_orders[i].buyer + "   " + g_orders[i].age;
    if (inf.length() > 46) inf = inf.substring(0, 46);
    tft.print(inf);
  }
}

// ═══════════════════════════════════════════════════════════
//  PAGE 2: NEWS
// ═══════════════════════════════════════════════════════════
void drawPageNews() {
  int x = 6, y = CONTENT_Y + 6;

  tft.setTextColor(C_CYAN, C_BG);
  tft.setTextSize(2);
  tft.setCursor(x+2, y);
  tft.print("TIN TUC MOI NHAT");
  y += 26;
  tft.drawFastHLine(0, y, SCREEN_W, C_CYAN);
  y += 6;

  if (g_news_count == 0) {
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextSize(1);
    tft.setCursor(x, y+30);
    tft.print("Chua co tin tuc nao.");
    return;
  }

  int cardH = (CONTENT_H - 40) / 3;
  const uint16_t catColors[3] = { C_CYAN, C_GREEN, C_MAGENTA };

  for (int i = 0; i < g_news_count && i < 3; i++) {
    int cy = y + i*(cardH + 3);
    uint16_t cbg = (i % 2 == 0) ? 0x0008 : 0x0420;
    uint16_t cc  = catColors[i % 3];

    fillBar(x, cy, SCREEN_W-12, cardH, cbg);
    tft.drawRect(x, cy, SCREEN_W-12, cardH, cc);

    // Badge category
    tft.setTextSize(1);
    String cat = g_news[i].cat;
    if (cat.length() > 10) cat = cat.substring(0, 10);
    int catW = cat.length() * 6 + 10;
    fillBar(x+2, cy+3, catW, 16, cc);
    tft.setTextColor(C_BG, cc);
    tft.setCursor(x+6, cy+5);
    tft.print(cat);

    // Tiêu đề (2 dòng nếu cần)
    tft.setTextColor(C_WHITE, cbg);
    String title = g_news[i].title;
    int titleX = catW + x + 8;

    if (title.length() <= 32) {
      tft.setCursor(titleX, cy + 7);
      tft.print(title);
    } else {
      tft.setCursor(titleX, cy + 3);
      tft.print(title.substring(0, 32));
      tft.setCursor(x+4, cy + 18);
      String rest = title.substring(32);
      if (rest.length() > 54) rest = rest.substring(0, 54) + "..";
      tft.print(rest);
    }

    // Thời gian + link
    tft.setTextColor(C_GRAY, cbg);
    tft.setCursor(x+4, cy + cardH - 13);
    tft.print(g_news[i].age + " truoc");

    tft.setTextColor(0x07BF, cbg);
    tft.setCursor(SCREEN_W - 115, cy + cardH - 13);
    tft.print("electroshop-ten.vercel.app");
  }
}

// ═══════════════════════════════════════════════════════════
//  PAGE 3: STATS
// ═══════════════════════════════════════════════════════════
void drawPageStats() {
  int x = 6, y = CONTENT_Y + 6;

  tft.setTextColor(C_MAGENTA, C_BG);
  tft.setTextSize(2);
  tft.setCursor(x+2, y);
  tft.print("THONG KE  &  PI 5");
  y += 26;
  tft.drawFastHLine(0, y, SCREEN_W, C_MAGENTA);
  y += 6;

  // ── 3 stat cards ──
  struct { const char* lbl; int val; uint16_t color; } cards[3] = {
    { "SAN PHAM",  g_prod_total,    C_CYAN   },
    { "DON HANG",  g_orders_total,  C_ORANGE },
    { "TIN TUC",   g_news_total,    C_GREEN  },
  };
  int cw = (SCREEN_W - 16) / 3 - 4;
  for (int i = 0; i < 3; i++) {
    int cx = x + i*(cw+6);
    uint16_t cbg = 0x0C42;
    fillBar(cx, y, cw, 64, cbg);
    tft.drawRect(cx, y, cw, 64, cards[i].color);
    tft.setTextColor(cards[i].color, cbg);
    tft.setTextSize(1);
    int lw = strlen(cards[i].lbl) * 6;
    tft.setCursor(cx + (cw-lw)/2, y+5);
    tft.print(cards[i].lbl);
    tft.setTextSize(3);
    String ns = String(cards[i].val);
    int nw = ns.length() * 18;
    tft.setCursor(cx + (cw-nw)/2, y+22);
    tft.print(ns);
  }

  y += 72;

  // ── Pi 5 System Info ──
  tft.drawFastHLine(x, y-2, SCREEN_W-12, 0x2104);

  // CPU Temp
  tft.setTextColor(C_LGRAY, C_BG);
  tft.setTextSize(1);
  tft.setCursor(x, y+2);
  tft.print("Pi5 CPU:");
  uint16_t tempColor = (g_cpu_temp < 65) ? C_GREEN : (g_cpu_temp < 75) ? C_YELLOW : C_RED;
  tft.setTextColor(tempColor, C_BG);
  tft.setCursor(x+55, y+2);
  char tempBuf[10];
  dtostrf(g_cpu_temp, 4, 1, tempBuf);
  tft.print(tempBuf); tft.print("C");

  // CPU bar
  drawBar(x+110, y+2, 110, 11, g_cpu_pct, pctColor(g_cpu_pct), 0x1082);
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(x+225, y+2);
  tft.print(String(g_cpu_pct) + "%");

  y += 16;

  // RAM
  tft.setTextColor(C_LGRAY, C_BG);
  tft.setCursor(x, y);
  tft.print("RAM:");
  drawBar(x+30, y, 150, 11, g_ram_pct, pctColor(g_ram_pct), 0x1082);
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(x+186, y);
  tft.print(String(g_ram_pct) + "%");

  // Disk
  tft.setTextColor(C_LGRAY, C_BG);
  tft.setCursor(x+240, y);
  tft.print("Disk:");
  drawBar(x+278, y, 100, 11, g_disk_pct, pctColor(g_disk_pct), 0x1082);
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(x+384, y);
  tft.print(String(g_disk_pct) + "%");

  y += 16;

  // IP + Uptime
  unsigned long up = (millis() - bootTime) / 1000;
  char uptimeBuf[24];
  sprintf(uptimeBuf, "Up: %02luh%02lum%02lus", up/3600, (up%3600)/60, up%60);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.setCursor(x, y);
  tft.print(uptimeBuf);

  if (g_pi_ip.length() > 0) {
    tft.setTextColor(C_TEAL, C_BG);
    tft.setCursor(x+160, y);
    tft.print("IP: " + g_pi_ip);
  }

  y += 16;

  // Links
  tft.setTextColor(0x47DF, C_BG);
  tft.setCursor(x, y);
  tft.print("Web: https://electroshop-ten.vercel.app");
  y += 13;
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(x, y);
  tft.print("Repo: github.com/Thooooooo/electroshop");
}

// ═══════════════════════════════════════════════════════════
//  FULL REDRAW
// ═══════════════════════════════════════════════════════════
void drawCurrentPage() {
  fillBar(0, CONTENT_Y+2, SCREEN_W, CONTENT_H, C_BG);
  switch (currentPage) {
    case PAGE_HOME:   drawPageHome();   break;
    case PAGE_ORDERS: drawPageOrders(); break;
    case PAGE_NEWS:   drawPageNews();   break;
    case PAGE_STATS:  drawPageStats();  break;
  }
  drawNavBar();
}

void fullRedraw() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawCurrentPage();
}

// ═══════════════════════════════════════════════════════════
//  JSON PARSER
// ═══════════════════════════════════════════════════════════
void parseJson(String& raw) {
  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, raw) != DeserializationError::Ok) return;

  if (doc["t"])           g_time         = doc["t"].as<String>();
  if (doc["d"])           g_date         = doc["d"].as<String>();
  if (doc["tunnel"])      g_tunnel       = doc["tunnel"].as<String>();
  if (doc["tunnel_url"])  g_tunnel_url   = doc["tunnel_url"].as<String>();
  if (doc["flask"])       g_flask        = doc["flask"].as<String>();
  if (doc["pb"])          g_pb           = doc["pb"].as<String>();

  // Stats
  if (doc["stats"]) {
    g_prod_total    = doc["stats"]["products"]     | 0;
    g_orders_total  = doc["stats"]["orders_total"] | 0;
    g_news_total    = doc["stats"]["news_total"]   | 0;
  }

  // Pi system
  if (doc["sys"]) {
    g_cpu_temp = doc["sys"]["cpu_temp"] | 0.0f;
    g_ram_pct  = doc["sys"]["ram_pct"]  | 0;
    g_disk_pct = doc["sys"]["disk_pct"] | 0;
    g_cpu_pct  = doc["sys"]["cpu_pct"]  | 0;
    g_pi_ip    = doc["sys"]["ip"]       | "";
  }

  // Orders
  if (doc["orders"]) {
    JsonArray arr = doc["orders"].as<JsonArray>();
    g_order_count = 0;
    for (JsonObject o : arr) {
      if (g_order_count >= 3) break;
      g_orders[g_order_count] = {
        o["name"]  | "Unknown",
        o["price"] | "0",
        o["by"]    | "?",
        o["age"]   | "?"
      };
      g_order_count++;
    }
  }

  // News
  if (doc["news"]) {
    JsonArray arr = doc["news"].as<JsonArray>();
    g_news_count = 0;
    for (JsonObject n : arr) {
      if (g_news_count >= 3) break;
      g_news[g_news_count] = {
        n["title"] | "No title",
        n["cat"]   | "?",
        n["age"]   | "?"
      };
      g_news_count++;
    }
  }

  needRedraw = true;
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // Backlight
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.init();
  tft.setRotation(1);   // Landscape 480×320
  tft.fillScreen(C_BG);
  tft.setTouch(calData);

  bootTime = millis();

  // Splash screen
  tft.setTextColor(C_CYAN, C_BG);
  tft.setTextSize(3);
  tft.setCursor(90, 90);
  tft.print("ELECTROSHOP");
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(130, 140);
  tft.print("CYD Dashboard v1.0  |  480x320 ILI9488");
  tft.setCursor(160, 158);
  tft.print("Waiting for Pi 5 data...");
  tft.setTextColor(C_DKGREEN, C_BG);
  tft.setCursor(148, 185);
  tft.print("Serial @ 115200 baud  /dev/ttyUSB0");

  delay(2500);
  fullRedraw();
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  // Đọc Serial (JSON newline-terminated)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 5 && serialBuffer[0] == '{') {
        parseJson(serialBuffer);
      }
      serialBuffer = "";
    } else {
      if (serialBuffer.length() < 2048) serialBuffer += c;
    }
  }

  // Touch — chuyển trang
  uint16_t tx, ty;
  if (tft.getTouch(&tx, &ty)) {
    delay(25);
    if (tft.getTouch(&tx, &ty)) {
      if (ty >= NAV_Y) {
        int newPage = (int)tx / (SCREEN_W / NUM_PAGES);
        if (newPage >= 0 && newPage < NUM_PAGES && newPage != currentPage) {
          currentPage = newPage;
          needRedraw = true;
        }
      }
      while (tft.getTouch(&tx, &ty)) delay(10);
    }
  }

  // Vẽ lại khi có dữ liệu mới
  if (needRedraw) {
    fullRedraw();
    needRedraw = false;
    lastHeaderRedraw = millis();
    return;
  }

  // Cập nhật đồng hồ mỗi giây — chỉ vẽ vùng giờ (không flicker)
  if (millis() - lastHeaderRedraw >= 1000) {
    drawHeaderClock();
    // STATS page có uptime → redraw nếu đang ở đó
    if (currentPage == PAGE_STATS) {
      drawPageStats();
      drawNavBar();
    }
    lastHeaderRedraw = millis();
  }
}
