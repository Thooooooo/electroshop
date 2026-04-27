/*
 * CYD YouTube Player — SD Card Edition
 * ESP32 WROOM (khong PSRAM) + ILI9488 480x320 + XPT2046 + SD + DAC
 *
 * Kien truc:
 *   1. Go query -> Pi tim YouTube tra JSON 5 ket qua
 *   2. Chon video -> Pi transcode AVI (MJPEG 320x240 @15fps + PCM 8kHz)
 *   3. ESP32 download AVI -> SD /video.avi
 *   4. ESP32 parse AVI tu SD -> hien thi JPEG + phat audio qua DAC GPIO26
 */

#include <TFT_eSPI.h>
// Touch dung TFT_eSPI built-in (khong can XPT2046_Touchscreen)
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>

// --- WiFi / Server ----------------------------------------------------------
#define WIFI_SSID  "CAFE 669"
#define WIFI_PASS  "51268989"
#define PI_HOST    "192.168.1.2"
#define PI_PORT    9000

// --- Chan Touch (HSPI) -------------------------------------------------------
#define T_CS    33
#define T_CLK   25
#define T_MISO  39
#define T_MOSI  32
#define T_IRQ   36   // active-LOW khi co cham

// --- SD card (VSPI) ----------------------------------------------------------
#define SD_CS   5

// --- Audio DAC ---------------------------------------------------------------
#define AUDIO_PIN 26

// --- Man hinh ----------------------------------------------------------------
#define SCR_W   480
#define SCR_H   320
#define VIDEO_W 320
#define VIDEO_H 240
// Video can giua: x_offset = (480-320)/2 = 80, y_offset = 24
#define VIDEO_X 80
#define VIDEO_Y 24

// --- Ring buffer audio 8kHz, 2 giay -----------------------------------------
#define AUDIO_BUF_SIZE 16000
static uint8_t  audioBuf[AUDIO_BUF_SIZE];
static volatile int audioReadPos  = 0;
static volatile int audioWritePos = 0;

// --- Objects -----------------------------------------------------------------
TFT_eSPI        tft = TFT_eSPI();
// Calibration touch TFT_eSPI built-in
uint16_t calData[5] = {275, 3620, 264, 3532, 3};
hw_timer_t*     audioTimer = NULL;

// --- App state ---------------------------------------------------------------
#define BOOT_BTN 0   // Nút BOOT vật lý = GPIO0, dùng làm BACKSPACE
enum Screen { SCREEN_HOME, SCREEN_SEARCH, SCREEN_RESULTS, SCREEN_PLAYER, SCREEN_TRANSLATE, SCREEN_TRANSLATE_RESULT };
static Screen currentScreen = SCREEN_HOME;

// --- Ket qua tim kiem --------------------------------------------------------
struct VideoResult {
    char    id[16];
    char    title[44];
    int     duration;
    uint8_t* thumbData;  // JPEG bytes (malloc)
    size_t  thumbLen;
};
static VideoResult results[5];
static int resultCount   = 0;
static int selectedVideo = -1;

// --- Query input -------------------------------------------------------------
static char queryBuf[64];
static int  queryLen = 0;

// --- Translation app state ---------------------------------------------------
static char transSrcBuf[128];        // input text
static int  transLen    = 0;
static char transResult[256];        // translated text
static const char* LANG_SRC[]   = {"vi", "en", "vi", "en"};
static const char* LANG_DEST[]  = {"en", "vi", "zh-CN", "zh-CN"};
static const char* LANG_LABEL[] = {"VI->EN", "EN->VI", "VI->ZH", "EN->ZH"};
static int  transLangIdx = 0;        // current language pair

// --- Player state ------------------------------------------------------------
static volatile bool isPlaying  = false;
static volatile bool isPaused   = false;
static int           volume     = 160;  // 0-255
static TaskHandle_t  playerTask = NULL;

// --- Seek helpers ------------------------------------------------------------
static volatile int  seekFrames = 0;   // >0 fwd, <0 bwd
static volatile bool stopPlayer = false;

// Forward declarations
void drawHomeScreen();
void drawResultsScreen();
void drawPlayerScreen();
void downloadToSD(const char* videoId);
void startPlayer();
void drawTranslateScreen();
void drawTransInputBox();
void drawTransResultScreen();


// ===========================================================================
// ISR phat audio @ 8kHz (Timer 0)
// ===========================================================================
void IRAM_ATTR onAudioTimer() {
    if (audioReadPos != audioWritePos) {
        uint8_t sample = audioBuf[audioReadPos];
        audioReadPos = (audioReadPos + 1) % AUDIO_BUF_SIZE;
        // Scale volume: dich ve tam, nhan he so, tra ve 0-255
        int scaled = ((int)sample - 128) * volume / 255 + 128;
        dacWrite(AUDIO_PIN, (uint8_t)constrain(scaled, 0, 255));
    }
    // Khi buffer rong: giu muc giua (im lang)
}


// ===========================================================================
// TJpgDec callback -- ve tile JPEG len TFT
// ===========================================================================
bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}


// ===========================================================================
// Doc touch, tra true neu dang cham
// ===========================================================================
bool getTouch(int &x, int &y) {
    static unsigned long lastTouchMs = 0;
    static bool wasDown = false;
    uint16_t tx, ty;

    // Gate 1: Đọc áp lực vật lý qua Z axis TRƯỚC KHI gọi getTouch()
    // Mục đích: phá vỡ vòng lặp phantom của _pressTime trong TFT_eSPI.
    // Cơ chế lỗi: getTouch() valid → _pressTime = now+50ms → 50ms tiếp theo
    // threshold tự drop về 20 → noise kích hoạt "valid" → _pressTime reset ...
    // Nếu Z thực tế < 300 (không chạm thật), ta skip hoàn toàn, không để
    // _pressTime bao giờ được refresh bởi noise.
    uint16_t z = tft.getTouchRawZ();
    if (z < 300) {
        wasDown = false;
        return false;
    }

    // Gate 2: Validated read với threshold bình thường
    if (tft.getTouch(&tx, &ty, 400)) {
        if (!wasDown && (millis() - lastTouchMs > 300)) {
            x = tx; y = ty;
            wasDown = true;
            lastTouchMs = millis();
            return true;
        }
        wasDown = true;
        return false;
    }
    wasDown = false;
    return false;
}


// ===========================================================================
// Ve nut bo goc + label
// ===========================================================================
void drawButton(int x, int y, int w, int h, const char* label,
                uint16_t bg, uint16_t textColor = TFT_WHITE) {
    tft.fillRoundRect(x, y, w, h, 6, bg);
    tft.drawRoundRect(x, y, w, h, 6, TFT_WHITE);
    tft.setTextColor(textColor, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + w / 2, y + h / 2);
}

bool buttonPressed(int tx, int ty, int bx, int by, int bw, int bh) {
    return (tx >= bx) && (tx <= bx + bw) && (ty >= by) && (ty <= by + bh);
}


// ===========================================================================
// Overlay thong bao loi (nen do)
// ===========================================================================
void showError(const char* msg) {
    tft.fillRoundRect(40, 120, 400, 80, 10, TFT_RED);
    tft.drawRoundRect(40, 120, 400, 80, 10, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.drawString(msg, 240, 160);
    delay(2000);
}


// ===========================================================================
// Overlay loading voi progress bar xanh
// ===========================================================================
void showLoading(const char* msg, int progress) {
    tft.fillRoundRect(40, 110, 400, 100, 10, TFT_NAVY);
    tft.drawRoundRect(40, 110, 400, 100, 10, TFT_CYAN);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.drawString(msg, 240, 140);
    // Progress bar
    tft.drawRoundRect(60, 170, 360, 20, 4, TFT_WHITE);
    int barW = (int)(360.0f * progress / 100.0f);
    if (barW > 0) tft.fillRoundRect(61, 171, barW, 18, 3, TFT_GREEN);
}


// ===========================================================================
// Base64 decode (RFC 4648)
// ===========================================================================
static const int8_t b64Table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

size_t base64Decode(const char* in, uint8_t* out, size_t inLen) {
    size_t outPos = 0;
    uint32_t acc  = 0;
    int      bits = 0;
    for (size_t i = 0; i < inLen; i++) {
        int8_t v = b64Table[(uint8_t)in[i]];
        if (v < 0) continue;
        acc = (acc << 6) | (uint8_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[outPos++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    return outPos;
}


// ===========================================================================
// Giai phong bo nho thumbnail cu
// ===========================================================================
void freeResults() {
    for (int i = 0; i < 5; i++) {
        if (results[i].thumbData) {
            free(results[i].thumbData);
            results[i].thumbData = nullptr;
            results[i].thumbLen  = 0;
        }
    }
    resultCount = 0;
}


// ===========================================================================
// Format thoi gian MM:SS
// ===========================================================================
void formatDuration(int secs, char* buf) {
    if (secs <= 0) { strcpy(buf, "--:--"); return; }
    snprintf(buf, 8, "%02d:%02d", secs / 60, secs % 60);
}


// ===========================================================================
// SCREEN_SEARCH -- ban phim on-screen
// ===========================================================================

static const char* ROW_NUMS = "1234567890";
static const char* ROW_TOP  = "qwertyuiop";
static const char* ROW_MID  = "asdfghjkl";
static const char* ROW_BOT  = "zxcvbnm";

#define KEY_W   42
#define KEY_H   38
#define KEY_Y0  90
#define KEY_GAP 2
#define TRANS_KEY_Y0  86

void drawKeyRow(const char* keys, int xStart, int y) {
    tft.setTextFont(2);
    tft.setTextSize(1);
    int n = strlen(keys);
    for (int i = 0; i < n; i++) {
        char label[2] = {keys[i], 0};
        drawButton(xStart + i * (KEY_W + KEY_GAP), y, KEY_W, KEY_H, label, 0x2945);
    }
}

void drawInputBox() {
    tft.fillRect(0, 40, SCR_W, 50, TFT_BLACK);
    tft.fillRoundRect(4, 44, SCR_W - 8, 42, 6, 0x1082);
    tft.drawRoundRect(4, 44, SCR_W - 8, 42, 6, TFT_CYAN);
    tft.setTextColor(TFT_WHITE, 0x1082);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    char disp[68];
    snprintf(disp, sizeof(disp), "%s_", queryBuf);
    tft.drawString(disp, 12, 65);
}

void drawSearchScreen() {
    tft.fillScreen(TFT_BLACK);

    // Header do YouTube
    tft.fillRect(0, 0, SCR_W, 40, 0xC800);
    tft.setTextColor(TFT_WHITE, 0xC800);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(4);
    tft.drawString("YouTube Player", 40, 20);

    drawInputBox();

    // Hang so (bỏ backspace khỏi đây)
    drawKeyRow(ROW_NUMS, 2, KEY_Y0);

    // Hang QWERTY
    drawKeyRow(ROW_TOP, 2, KEY_Y0 + KEY_H + KEY_GAP);

    // Hang ASDF (can giua)
    int xMid = (SCR_W - (int)strlen(ROW_MID) * (KEY_W + KEY_GAP)) / 2;
    drawKeyRow(ROW_MID, xMid, KEY_Y0 + 2 * (KEY_H + KEY_GAP));

    // Hang ZXCV + SPACE + SEARCH (bo backspace)
    drawKeyRow(ROW_BOT, 2, KEY_Y0 + 3 * (KEY_H + KEY_GAP));
    int xAfterBot = 2 + (int)strlen(ROW_BOT) * (KEY_W + KEY_GAP) + KEY_GAP;
    tft.setTextFont(2);
    drawButton(xAfterBot,      KEY_Y0 + 3*(KEY_H+KEY_GAP), 90,  KEY_H, "SPC",    0x2945);
    drawButton(xAfterBot + 92, KEY_Y0 + 3*(KEY_H+KEY_GAP), 120, KEY_H, "SEARCH", 0xC800);
}

// Tra ve ky tu duoc bam, 0 neu khong co, -1=backspace, -2=search
int getTappedKey(int tx, int ty) {
    // DEAD ZONE: goc phai man hinh x>430 (vung loi cam ung)
    if (tx > 430) return 0;

    // Hang so (khong co backspace o day nua)
    if (ty >= KEY_Y0 && ty < KEY_Y0 + KEY_H) {
        for (int i = 0; i < 10; i++) {
            int x = 2 + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_NUMS[i];
        }
    }
    // Hang TOP
    int yTop = KEY_Y0 + KEY_H + KEY_GAP;
    if (ty >= yTop && ty < yTop + KEY_H) {
        for (int i = 0; i < 10; i++) {
            int x = 2 + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_TOP[i];
        }
    }
    // Hang MID
    int yMid = KEY_Y0 + 2 * (KEY_H + KEY_GAP);
    int xMidStart = (SCR_W - (int)strlen(ROW_MID) * (KEY_W + KEY_GAP)) / 2;
    if (ty >= yMid && ty < yMid + KEY_H) {
        for (int i = 0; i < (int)strlen(ROW_MID); i++) {
            int x = xMidStart + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_MID[i];
        }
    }
    // Hang BOT + [<=] + SPACE + SEARCH
    int yBot = KEY_Y0 + 3 * (KEY_H + KEY_GAP);
    if (ty >= yBot && ty < yBot + KEY_H) {
        for (int i = 0; i < (int)strlen(ROW_BOT); i++) {
            int x = 2 + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_BOT[i];
        }
        int xAfter = 2 + (int)strlen(ROW_BOT) * (KEY_W + KEY_GAP) + KEY_GAP;
        if (tx >= xAfter && tx < xAfter + 90)         return ' ';   // space
        if (tx >= xAfter + 92 && tx < xAfter + 212)  return -2;    // search
    }
    return 0;
}


// --- doSearch: goi Pi API, parse JSON, luu results[] -----------------------
void doSearch() {
    if (queryLen == 0) return;

    showLoading("Dang tim kiem...", 20);

    // URL encode query (space -> +, ky tu dac biet -> %XX)
    char encoded[128] = {0};
    int ei = 0;
    for (int i = 0; i < queryLen && ei < 120; i++) {
        char c = queryBuf[i];
        if (c == ' ') {
            encoded[ei++] = '+';
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '_') {
            encoded[ei++] = c;
        } else {
            snprintf(encoded + ei, 4, "%%%02X", (unsigned char)c);
            ei += 3;
        }
    }
    encoded[ei] = 0;

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/search?q=%s",
             PI_HOST, PI_PORT, encoded);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        showError("Loi ket noi den Pi!");
        return;
    }

    showLoading("Dang xu ly ket qua...", 60);

    // Parse JSON stream de tiet kiem RAM
    DynamicJsonDocument doc(32768);
    WiFiClient* stream = http.getStreamPtr();
    DeserializationError err = deserializeJson(doc, *stream);
    http.end();

    if (err) {
        showError("JSON parse loi!");
        return;
    }

    freeResults();
    JsonArray arr = doc.as<JsonArray>();
    resultCount = 0;

    for (JsonObject obj : arr) {
        if (resultCount >= 5) break;
        int i = resultCount;

        strlcpy(results[i].id,    obj["id"]    | "", sizeof(results[i].id));
        strlcpy(results[i].title, obj["title"] | "", sizeof(results[i].title));
        results[i].duration  = obj["duration"] | 0;
        results[i].thumbData = nullptr;
        results[i].thumbLen  = 0;

        // Decode thumbnail base64 -> JPEG bytes
        const char* thumbB64 = obj["thumb"] | "";
        size_t b64Len = strlen(thumbB64);
        if (b64Len > 10) {
            size_t maxOut = (b64Len / 4) * 3 + 4;
            uint8_t* buf = (uint8_t*) malloc(maxOut);
            if (buf) {
                size_t outLen = base64Decode(thumbB64, buf, b64Len);
                results[i].thumbData = buf;
                results[i].thumbLen  = outLen;
            }
        }
        resultCount++;
    }

    currentScreen = SCREEN_RESULTS;
}


// --- handleSearchTouch ------------------------------------------------------
static bool lastTouched     = false;
static unsigned long lastMs = 0;

void handleSearchTouch(int tx, int ty, bool touched) {
    if (!touched) { lastTouched = false; return; }
    if (lastTouched) return;
    if (millis() - lastMs < 150) return;
    lastTouched = true;
    lastMs = millis();

    int key = getTappedKey(tx, ty);
    if (key == 0) return;

    if (key == -1) {
        if (queryLen > 0) queryBuf[--queryLen] = 0;
        drawInputBox();
    } else if (key == -2) {
        doSearch();
        if (currentScreen == SCREEN_RESULTS) drawResultsScreen();
    } else {
        if (queryLen < 63) {
            queryBuf[queryLen++] = (char)key;
            queryBuf[queryLen]   = 0;
        }
        drawInputBox();
    }
}


// ===========================================================================
// SCREEN_HOME -- Chon ung dung
// ===========================================================================
void drawHomeScreen() {
    tft.fillScreen(TFT_BLACK);
    // Header
    tft.fillRect(0, 0, SCR_W, 50, 0x1082);
    tft.setTextColor(TFT_CYAN, 0x1082);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString("CYD Hub", SCR_W/2, 25);

    // YouTube button (red)
    tft.fillRoundRect(40, 80, 180, 140, 16, 0xC800);
    tft.drawRoundRect(40, 80, 180, 140, 16, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, 0xC800);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString("YouTube", 130, 140);
    tft.setTextFont(2);
    tft.drawString("xem video", 130, 170);

    // Translate button (green)
    tft.fillRoundRect(260, 80, 180, 140, 16, 0x0600);
    tft.drawRoundRect(260, 80, 180, 140, 16, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, 0x0600);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString("Dich", 350, 140);
    tft.setTextFont(2);
    tft.drawString("dich van ban", 350, 170);
}

void handleHomeTouch(int tx, int ty, bool touched) {
    if (!touched) return;
    // YouTube: x 40-220, y 80-220
    if (tx >= 40 && tx <= 220 && ty >= 80 && ty <= 220) {
        queryBuf[0] = 0; queryLen = 0;
        currentScreen = SCREEN_SEARCH;
        drawSearchScreen();
        return;
    }
    // Translate: x 260-440, y 80-220
    if (tx >= 260 && tx <= 440 && ty >= 80 && ty <= 220) {
        transSrcBuf[0] = 0; transLen = 0;
        transResult[0] = 0;
        currentScreen = SCREEN_TRANSLATE;
        drawTranslateScreen();
        return;
    }
}


// ===========================================================================
// SCREEN_RESULTS -- danh sach ket qua
// ===========================================================================

void drawResultsScreen() {
    tft.fillScreen(TFT_BLACK);

    // Header
    tft.fillRect(0, 0, SCR_W, 40, 0x2945);
    tft.setTextColor(TFT_WHITE, 0x2945);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "< Back   Ket qua: \"%s\"", queryBuf);
    tft.drawString(hdr, 8, 20);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpegOutput);

    for (int i = 0; i < resultCount; i++) {
        int yBase = 40 + i * 56;
        tft.fillRect(0, yBase, SCR_W, 55, (i % 2) ? 0x1082 : 0x0841);

        // Thumbnail 120x50 (scale tu 120x68 cho vua 55px row)
        if (results[i].thumbData && results[i].thumbLen > 0) {
            TJpgDec.drawJpg(4, yBase + 2, results[i].thumbData, results[i].thumbLen);
        } else {
            tft.fillRect(4, yBase + 2, 120, 50, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.setTextFont(1);
            tft.drawString("No img", 64, yBase + 27);
        }

        // Title (toi da 2 dong x 28 ky tu)
        tft.setTextColor(TFT_WHITE, (i % 2) ? 0x1082 : 0x0841);
        tft.setTextDatum(ML_DATUM);
        tft.setTextFont(2);
        char line1[30], line2[30];
        int titleLen = strlen(results[i].title);
        strncpy(line1, results[i].title, 28); line1[28] = 0;
        if (titleLen > 28) {
            strncpy(line2, results[i].title + 28, 28); line2[28] = 0;
        } else {
            line2[0] = 0;
        }
        tft.drawString(line1, 132, yBase + 6);
        if (line2[0]) tft.drawString(line2, 132, yBase + 22);

        // Duration MM:SS
        char dur[10];
        formatDuration(results[i].duration, dur);
        tft.setTextColor(TFT_LIGHTGREY, (i % 2) ? 0x1082 : 0x0841);
        tft.setTextFont(1);
        tft.drawString(dur, 132, yBase + 40);

        // Nut Play
        tft.setTextFont(2);
        drawButton(SCR_W - 72, yBase + 8, 68, 36, "> Play", 0xC800);
    }
}

// --- prepareVideo: goi /prepare, poll /status, roi download -----------------
void prepareVideo(const char* videoId) {
    char url[128];

    // Goi /prepare
    snprintf(url, sizeof(url), "http://%s:%d/prepare/%s",
             PI_HOST, PI_PORT, videoId);
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    int code = http.GET();
    http.end();

    if (code != 200) {
        showError("Loi goi /prepare!");
        return;
    }

    // Poll /status moi 2 giay, toi da 6 phut
    snprintf(url, sizeof(url), "http://%s:%d/status/%s",
             PI_HOST, PI_PORT, videoId);

    const char* spins[] = {"|", "/", "-", "\\"};
    int attempt = 0;
    bool ready = false;

    while (attempt < 180) {
        char spinMsg[48];
        snprintf(spinMsg, sizeof(spinMsg), "%s Pi dang transcode...",
                 spins[attempt % 4]);
        showLoading(spinMsg, min(attempt * 90 / 180, 90));

        http.begin(url);
        http.setTimeout(5000);
        code = http.GET();
        if (code == 200) {
            String body = http.getString();
            DynamicJsonDocument doc(256);
            if (!deserializeJson(doc, body)) {
                const char* status = doc["status"] | "unknown";
                if (strcmp(status, "ready") == 0)  { ready = true; break; }
                if (strcmp(status, "error") == 0)  {
                    http.end();
                    showError("Pi bao loi transcode!");
                    return;
                }
            }
        }
        http.end();
        attempt++;
        delay(2000);
    }

    if (!ready) {
        showError("Timeout cho Pi transcode!");
        return;
    }
    http.end();

    downloadToSD(videoId);
}

// --- downloadToSD: stream AVI ve SD -----------------------------------------
void downloadToSD(const char* videoId) {
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/download/%s",
             PI_HOST, PI_PORT, videoId);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(120000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        showError("Loi download AVI!");
        return;
    }

    int contentLen = http.getSize();

    // Xoa file cu neu co
    if (SD.exists("/video.avi")) SD.remove("/video.avi");
    File f = SD.open("/video.avi", FILE_WRITE);
    if (!f) {
        http.end();
        showError("Khong mo duoc SD!");
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[4096];
    int totalRead = 0;
    int lastProgress = -1;

    // Doc chunks, ghi vao SD, hien thi progress
    while (http.connected() || stream->available()) {
        int avail = stream->available();
        if (avail == 0) { delay(1); continue; }
        size_t toRead = (avail > (int)sizeof(buf)) ? sizeof(buf) : avail;
        size_t n = stream->readBytes(buf, toRead);
        if (n == 0) break;
        f.write(buf, n);
        totalRead += (int)n;

        int progress = (contentLen > 0)
                       ? (int)((long)totalRead * 100 / contentLen)
                       : (totalRead / 10240) % 99;
        if (progress != lastProgress) {
            char msg[40];
            snprintf(msg, sizeof(msg), "Dang tai: %d%%", progress);
            showLoading(msg, progress);
            lastProgress = progress;
        }
    }

    f.close();
    http.end();

    showLoading("Tai xong! Dang khoi dong...", 100);
    delay(500);
    startPlayer();
}

void handleResultsTouch(int tx, int ty, bool touched) {
    if (!touched) { lastTouched = false; return; }
    if (lastTouched) return;
    if (millis() - lastMs < 200) return;
    lastTouched = true;
    lastMs = millis();

    // Back button (header)
    if (ty < 40 && tx < 80) {
        freeResults();
        currentScreen = SCREEN_SEARCH;
        drawSearchScreen();
        return;
    }

    // Item rows -- nut Play
    for (int i = 0; i < resultCount; i++) {
        int yBase = 40 + i * 56;
        if (buttonPressed(tx, ty, SCR_W - 72, yBase + 8, 68, 36)) {
            selectedVideo = i;
            tft.fillScreen(TFT_BLACK);
            showLoading("Chuan bi video...", 5);
            prepareVideo(results[i].id);
            return;
        }
    }
}


// ===========================================================================
// SCREEN_TRANSLATE -- ban phim dich thuat
// ===========================================================================

void drawTransInputBox() {
    tft.fillRect(0, 40, SCR_W, 46, TFT_BLACK);
    tft.fillRoundRect(4, 42, SCR_W - 8, 42, 6, 0x1082);
    tft.drawRoundRect(4, 42, SCR_W - 8, 42, 6, TFT_YELLOW);
    tft.setTextColor(TFT_WHITE, 0x1082);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    char disp[130];
    snprintf(disp, sizeof(disp), "%s_", transSrcBuf);
    tft.drawString(disp, 10, 63);
}

void drawTranslateScreen() {
    tft.fillScreen(TFT_BLACK);
    // Header
    tft.fillRect(0, 0, SCR_W, 40, 0x0442);
    tft.setTextColor(TFT_WHITE, 0x0442);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.drawString("Dich Thuat", 8, 20);
    // Language toggle button
    drawButton(180, 4, 100, 32, LANG_LABEL[transLangIdx], 0x0260, TFT_WHITE);
    // Back button
    drawButton(390, 4, 86, 32, "< Home", 0x2945, TFT_WHITE);

    drawTransInputBox();

    int y0 = TRANS_KEY_Y0;
    drawKeyRow(ROW_NUMS, 2, y0);
    drawKeyRow(ROW_TOP,  2, y0 + KEY_H + KEY_GAP);
    int xMid = (SCR_W - (int)strlen(ROW_MID) * (KEY_W + KEY_GAP)) / 2;
    drawKeyRow(ROW_MID, xMid, y0 + 2*(KEY_H + KEY_GAP));
    drawKeyRow(ROW_BOT,  2, y0 + 3*(KEY_H + KEY_GAP));
    int xAfterBot = 2 + (int)strlen(ROW_BOT) * (KEY_W + KEY_GAP) + KEY_GAP;
    drawButton(xAfterBot,       y0 + 3*(KEY_H+KEY_GAP), 60,  KEY_H, "SPC",  0x2945);
    drawButton(xAfterBot + 62,  y0 + 3*(KEY_H+KEY_GAP), 60,  KEY_H, "CLR",  0x4000);
    drawButton(xAfterBot + 124, y0 + 3*(KEY_H+KEY_GAP), 100, KEY_H, "DICH", 0x0260);
}

// Returns char >= 32 = key, -1=CLR, -2=DICH, 0=nothing
int getTappedKeyTrans(int tx, int ty) {
    if (tx > 430) return 0;
    int y0 = TRANS_KEY_Y0;
    // Row nums
    if (ty >= y0 && ty < y0 + KEY_H) {
        for (int i = 0; i < 10; i++) {
            int x = 2 + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_NUMS[i];
        }
    }
    int yTop = y0 + KEY_H + KEY_GAP;
    if (ty >= yTop && ty < yTop + KEY_H) {
        for (int i = 0; i < 10; i++) {
            int x = 2 + i * (KEY_W + KEY_GAP);
            if (tx >= x && tx < x + KEY_W) return ROW_TOP[i];
        }
    }
    int yMid = y0 + 2*(KEY_H + KEY_GAP);
    int xMidStart = (SCR_W - (int)strlen(ROW_MID)*(KEY_W+KEY_GAP))/2;
    if (ty >= yMid && ty < yMid + KEY_H) {
        for (int i = 0; i < (int)strlen(ROW_MID); i++) {
            int x = xMidStart + i*(KEY_W+KEY_GAP);
            if (tx >= x && tx < x+KEY_W) return ROW_MID[i];
        }
    }
    int yBot = y0 + 3*(KEY_H + KEY_GAP);
    if (ty >= yBot && ty < yBot + KEY_H) {
        for (int i = 0; i < (int)strlen(ROW_BOT); i++) {
            int x = 2 + i*(KEY_W+KEY_GAP);
            if (tx >= x && tx < x+KEY_W) return ROW_BOT[i];
        }
        int xAfter = 2 + (int)strlen(ROW_BOT)*(KEY_W+KEY_GAP) + KEY_GAP;
        if (tx >= xAfter && tx < xAfter + 60)        return ' ';
        if (tx >= xAfter+62 && tx < xAfter+122)      return -1;  // CLR
        if (tx >= xAfter+124 && tx < xAfter+224)     return -2;  // DICH
    }
    return 0;
}

void doTranslate() {
    if (transLen == 0) return;
    showLoading("Dang dich...", 30);

    // URL-encode
    char encoded[200] = {0};
    int ei = 0;
    for (int i = 0; i < transLen && ei < 190; i++) {
        char c = transSrcBuf[i];
        if (c == ' ') { encoded[ei++] = '+'; }
        else if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_') { encoded[ei++] = c; }
        else { snprintf(encoded+ei, 4, "%%%02X", (unsigned char)c); ei+=3; }
    }
    encoded[ei] = 0;

    char url[320];
    snprintf(url, sizeof(url), "http://%s:%d/translate?text=%s&src=%s&dest=%s",
             PI_HOST, PI_PORT, encoded, LANG_SRC[transLangIdx], LANG_DEST[transLangIdx]);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) { http.end(); showError("Loi ket noi Pi!"); return; }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, *http.getStreamPtr());
    http.end();
    if (err || doc.containsKey("error")) { showError("Loi dich thuat!"); return; }

    const char* t = doc["translated"] | "";
    strlcpy(transResult, t, sizeof(transResult));
    currentScreen = SCREEN_TRANSLATE_RESULT;
    drawTransResultScreen();
}

void handleTranslateTouch(int tx, int ty, bool touched) {
    if (!touched) return;
    // Back button: x 390-476, y 4-36
    if (tx >= 390 && ty >= 4 && ty <= 36) {
        currentScreen = SCREEN_HOME;
        drawHomeScreen();
        return;
    }
    // Language toggle: x 180-280, y 4-36
    if (tx >= 180 && tx <= 280 && ty >= 4 && ty <= 36) {
        transLangIdx = (transLangIdx + 1) % 4;
        drawButton(180, 4, 100, 32, LANG_LABEL[transLangIdx], 0x0260, TFT_WHITE);
        return;
    }
    // Keyboard
    int key = getTappedKeyTrans(tx, ty);
    if (key == 0) return;
    if (key == -1) {
        transSrcBuf[0] = 0; transLen = 0;
        drawTransInputBox();
    } else if (key == -2) {
        doTranslate();
    } else {
        if (transLen < 127) {
            transSrcBuf[transLen++] = (char)key;
            transSrcBuf[transLen]   = 0;
        }
        drawTransInputBox();
    }
}


// ===========================================================================
// SCREEN_TRANSLATE_RESULT
// ===========================================================================
void drawTransResultScreen() {
    tft.fillScreen(TFT_BLACK);
    // Header
    tft.fillRect(0, 0, SCR_W, 40, 0x0442);
    tft.setTextColor(TFT_WHITE, 0x0442);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "< %s", LANG_LABEL[transLangIdx]);
    tft.drawString(hdr, 8, 20);
    drawButton(370, 4, 100, 32, "Dich lai", 0x0260, TFT_WHITE);

    // Source text box (teal)
    tft.fillRoundRect(4, 44, SCR_W-8, 120, 6, 0x0442);
    tft.drawRoundRect(4, 44, SCR_W-8, 120, 6, TFT_CYAN);
    tft.setTextColor(0x8410, 0x0442);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(1);
    tft.drawString(LANG_SRC[transLangIdx], 12, 56);
    tft.setTextColor(TFT_WHITE, 0x0442);
    tft.setTextFont(2);
    int srcY = 68;
    const char* sp = transSrcBuf;
    char line[52];
    while (*sp && srcY < 158) {
        strncpy(line, sp, 50); line[50]=0;
        tft.drawString(line, 12, srcY);
        sp += strlen(line);
        srcY += 18;
    }

    // Divider
    tft.drawFastHLine(0, 168, SCR_W, TFT_DARKGREY);

    // Translated text box (dark green)
    tft.fillRoundRect(4, 172, SCR_W-8, 144, 6, 0x0240);
    tft.drawRoundRect(4, 172, SCR_W-8, 144, 6, TFT_GREEN);
    tft.setTextColor(0x8410, 0x0240);
    tft.setTextFont(1);
    tft.drawString(LANG_DEST[transLangIdx], 12, 184);
    tft.setTextColor(TFT_WHITE, 0x0240);
    tft.setTextFont(2);
    int dstY = 198;
    const char* dp = transResult;
    char dline[52];
    while (*dp && dstY < 308) {
        strncpy(dline, dp, 50); dline[50]=0;
        tft.drawString(dline, 12, dstY);
        dp += strlen(dline);
        dstY += 18;
    }
}

void handleTransResultTouch(int tx, int ty, bool touched) {
    if (!touched) return;
    // "Dich lai" button: x 370-470, y 4-36
    if (tx >= 370 && ty >= 4 && ty <= 36) {
        currentScreen = SCREEN_TRANSLATE;
        drawTranslateScreen();
        return;
    }
    // Click anywhere else below header -> back to translate input
    if (ty > 40) {
        currentScreen = SCREEN_TRANSLATE;
        drawTranslateScreen();
    }
}


// ===========================================================================
// SCREEN_PLAYER -- phat video tu SD
// ===========================================================================

void drawPlayerScreen() {
    tft.fillScreen(TFT_BLACK);

    // Title bar
    tft.fillRect(0, 0, SCR_W, VIDEO_Y, 0x2945);
    tft.setTextColor(TFT_WHITE, 0x2945);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(1);
    if (selectedVideo >= 0) {
        char title[62];
        strncpy(title, results[selectedVideo].title, 60);
        title[60] = 0;
        tft.drawString(title, 4, VIDEO_Y / 2);
    }

    // Control bar y=264
    tft.fillRect(0, 264, SCR_W, 56, 0x2945);
    tft.setTextFont(2);
    drawButton(  0, 264, 80, 56, "<<10s",             0x4208);
    drawButton( 80, 264, 80, 56, isPaused ? ">Play" : "||",  0xC800);
    drawButton(160, 264, 80, 56, "10s>>",             0x4208);
    drawButton(240, 264, 80, 56, "Vol-",              0x0C48);
    drawButton(320, 264, 80, 56, "Vol+",              0x0C48);
    drawButton(400, 264, 80, 56, "Stop",              TFT_DARKGREY);
}

// --- AVI parser task (Core 0) ------------------------------------------------
static void aviPlayerTask(void* param) {
    File avi = SD.open("/video.avi");
    if (!avi) {
        Serial.println("Khong mo duoc /video.avi");
        vTaskDelete(NULL);
        return;
    }

    // Bo qua 12 byte RIFF header
    uint8_t hdrBuf[8];
    avi.read(hdrBuf, 12);

    // Tim "movi" chunk trong toi da 256KB
    bool foundMovi = false;
    while (avi.position() < 262144) {
        if (avi.read(hdrBuf, 8) < 8) break;
        uint32_t sz = (uint32_t)hdrBuf[4]        |
                      ((uint32_t)hdrBuf[5] << 8)  |
                      ((uint32_t)hdrBuf[6] << 16) |
                      ((uint32_t)hdrBuf[7] << 24);
        if (memcmp(hdrBuf, "LIST", 4) == 0) {
            uint8_t sub[4];
            avi.read(sub, 4);
            if (memcmp(sub, "movi", 4) == 0) { foundMovi = true; break; }
            // LIST khac (hdrl, etc.) -> tiep tuc doc ben trong
        } else {
            // Chunk thuong -> skip (align even)
            uint32_t skip = sz + (sz & 1);
            avi.seek(avi.position() + skip);
        }
    }

    if (!foundMovi) {
        Serial.println("Khong tim thay movi!");
        avi.close();
        isPlaying = false;
        vTaskDelete(NULL);
        return;
    }

    // Frame buffer 64KB
    uint8_t* frameBuf = (uint8_t*) malloc(65536);
    if (!frameBuf) {
        Serial.println("Het RAM cho frameBuf!");
        avi.close();
        isPlaying = false;
        vTaskDelete(NULL);
        return;
    }

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpegOutput);

    unsigned long lastFrameMs = millis();
    isPlaying = true;

    // --- Loop doc AVI chunks ------------------------------------------------
    while (avi.available() && !stopPlayer) {

        // Xu ly pause
        while (isPaused && !stopPlayer) vTaskDelay(10);
        if (stopPlayer) break;

        // Xu ly seek (+/- frames)
        if (seekFrames != 0) {
            long frameBytes = 9216L;
            long offset     = (long)seekFrames * frameBytes;
            long newPos     = (long)avi.position() + offset;
            if (newPos < 0) newPos = 0;
            avi.seek((uint32_t)newPos);
            seekFrames = 0;
        }

        // Doc fourcc + chunk_size
        uint8_t cc[8];
        if (avi.read(cc, 8) < 8) break;
        uint32_t chunkSize = (uint32_t)cc[4]        |
                             ((uint32_t)cc[5] << 8)  |
                             ((uint32_t)cc[6] << 16) |
                             ((uint32_t)cc[7] << 24);

        if (memcmp(cc, "LIST", 4) == 0) {
            // LIST "rec " -> bo qua 4 byte sub-type, tiep tuc
            uint8_t sub[4];
            avi.read(sub, 4);
            continue;
        }

        if (memcmp(cc, "00dc", 4) == 0) {
            // --- Video frame (MJPEG) ----------------------------------------
            if (chunkSize > 0 && chunkSize <= 65536) {
                size_t got = avi.read(frameBuf, chunkSize);
                if (got == chunkSize) {
                    // Timing: doi du 67ms / frame (~15fps)
                    unsigned long now = millis();
                    long wait = 67L - (long)(now - lastFrameMs);
                    if (wait > 0) vTaskDelay(pdMS_TO_TICKS((uint32_t)wait));
                    lastFrameMs = millis();
                    // Decode JPEG va hien thi tai (VIDEO_X, VIDEO_Y)
                    TJpgDec.drawJpg(VIDEO_X, VIDEO_Y, frameBuf, (uint32_t)got);
                }
            } else {
                avi.seek(avi.position() + chunkSize);
            }
            // Padding
            if (chunkSize & 1) { uint8_t pad; avi.read(&pad, 1); }

        } else if (memcmp(cc, "01wb", 4) == 0) {
            // --- Audio chunk (PCM u8 8kHz) -----------------------------------
            uint32_t remain = chunkSize;
            while (remain > 0 && !stopPlayer) {
                uint8_t ab[256];
                uint32_t toRead = (remain < 256) ? remain : 256;
                size_t got = avi.read(ab, toRead);
                if (got == 0) break;
                for (size_t k = 0; k < got; ) {
                    int next = (audioWritePos + 1) % AUDIO_BUF_SIZE;
                    if (next != audioReadPos) {
                        audioBuf[audioWritePos] = ab[k];
                        audioWritePos = next;
                        k++;
                    } else {
                        vTaskDelay(1);  // buffer day, doi ISR tieu thu
                    }
                }
                remain -= (uint32_t)got;
            }
            if (chunkSize & 1) { uint8_t pad; avi.read(&pad, 1); }

        } else if (memcmp(cc, "idx1", 4) == 0) {
            // Index chunk -> het movi
            break;
        } else {
            // Chunk khong ro -> skip
            uint32_t skip = chunkSize + (chunkSize & 1);
            if (skip > 0) avi.seek(avi.position() + skip);
        }
    }

    free(frameBuf);
    avi.close();
    isPlaying  = false;
    stopPlayer = false;

    // Quay ve man hinh ket qua
    currentScreen = SCREEN_RESULTS;
    drawResultsScreen();

    vTaskDelete(NULL);
}

void startPlayer() {
    stopPlayer    = false;
    isPaused      = false;
    isPlaying     = false;
    seekFrames    = 0;
    audioReadPos  = 0;
    audioWritePos = 0;

    currentScreen = SCREEN_PLAYER;
    drawPlayerScreen();

    // AVI parser tren Core 0 (loop() chay tren Core 1)
    xTaskCreatePinnedToCore(
        aviPlayerTask, "aviPlayer",
        8192, NULL, 1, &playerTask, 0
    );
}

void handlePlayerTouch(int tx, int ty, bool touched) {
    if (!touched) { lastTouched = false; return; }
    if (lastTouched) return;
    if (millis() - lastMs < 200) return;
    lastTouched = true;
    lastMs = millis();

    if (ty < 264) return;  // Chi xu ly vung controls

    if (buttonPressed(tx, ty, 0, 264, 80, 56)) {
        seekFrames = -150;
        tft.fillRoundRect(0, 264, 80, 56, 6, TFT_YELLOW); delay(80);
        drawButton(0, 264, 80, 56, "<<10s", 0x4208);
    }
    else if (buttonPressed(tx, ty, 80, 264, 80, 56)) {
        isPaused = !isPaused;
        tft.fillRoundRect(80, 264, 80, 56, 6, TFT_YELLOW); delay(80);
        drawButton(80, 264, 80, 56, isPaused ? ">Play" : "||", 0xC800);
    }
    else if (buttonPressed(tx, ty, 160, 264, 80, 56)) {
        seekFrames = 150;
        tft.fillRoundRect(160, 264, 80, 56, 6, TFT_YELLOW); delay(80);
        drawButton(160, 264, 80, 56, "10s>>", 0x4208);
    }
    else if (buttonPressed(tx, ty, 240, 264, 80, 56)) {
        volume = constrain(volume - 30, 0, 255);
        tft.fillRoundRect(240, 264, 80, 56, 6, TFT_YELLOW); delay(80);
        drawButton(240, 264, 80, 56, "Vol-", 0x0C48);
    }
    else if (buttonPressed(tx, ty, 320, 264, 80, 56)) {
        volume = constrain(volume + 30, 0, 255);
        tft.fillRoundRect(320, 264, 80, 56, 6, TFT_YELLOW); delay(80);
        drawButton(320, 264, 80, 56, "Vol+", 0x0C48);
    }
    else if (buttonPressed(tx, ty, 400, 264, 80, 56)) {
        stopPlayer = true;
        isPaused   = false;
        tft.fillRoundRect(400, 264, 80, 56, 6, TFT_YELLOW); delay(80);
    }
}


// ===========================================================================
// setup()
// ===========================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("CYD YouTube Player boot...");

    // TFT
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Backlight ON + BOOT button as backspace
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);
    pinMode(BOOT_BTN, INPUT_PULLUP);

    // Touch TFT_eSPI built-in
    tft.setTouch(calData);

    // TJpgDec
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpegOutput);

    // SD card (VSPI, CS=5)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.drawString("Khoi tao SD...", 10, 60);
    if (!SD.begin(SD_CS)) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("SD FAILED! Kiem tra ket noi.", 10, 80);
        Serial.println("SD init failed!");
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("SD OK", 10, 80);
        Serial.println("SD OK");
    }

    // Audio timer @ 8kHz (Timer 0, prescaler 80)
    // 80MHz / 80 = 1MHz; alarm = 125 -> 8kHz
    // ESP32 Arduino core 3.x: timerBegin(freq_hz)
    audioTimer = timerBegin(8000);          // 8000 Hz = 8kHz
    timerAttachInterrupt(audioTimer, &onAudioTimer);
    timerAlarm(audioTimer, 1, true, 0);     // alarm mỗi 1 tick = 8kHz
    Serial.println("Audio timer 8kHz OK");

    // WiFi
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Dang ket noi WiFi...", 10, 100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi OK: ");
        Serial.println(WiFi.localIP());
        tft.fillScreen(TFT_BLACK);
        currentScreen = SCREEN_HOME;
        drawHomeScreen();
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WiFi FAILED!", 10, 140);
        Serial.println("WiFi failed!");
    }
}


// ===========================================================================
// loop() -- chay tren Core 1
// ===========================================================================
void loop() {
    int tx = 0, ty = 0;
    bool touched = getTouch(tx, ty);

    // BOOT button = backspace (hoat dong o Search va Translate)
    static bool bootWasDown = false;
    if (digitalRead(BOOT_BTN) == LOW) {
        if (!bootWasDown) {
            bootWasDown = true;
            if (currentScreen == SCREEN_SEARCH) {
                if (queryLen > 0) { queryBuf[--queryLen] = 0; drawInputBox(); }
                else { currentScreen = SCREEN_HOME; drawHomeScreen(); }
            } else if (currentScreen == SCREEN_TRANSLATE) {
                if (transLen > 0) { transSrcBuf[--transLen] = 0; drawTransInputBox(); }
            } else if (currentScreen == SCREEN_HOME) {
                // nothing
            } else {
                currentScreen = SCREEN_HOME;
                drawHomeScreen();
            }
        }
    } else { bootWasDown = false; }

    switch (currentScreen) {
        case SCREEN_HOME:             handleHomeTouch(tx, ty, touched);         break;
        case SCREEN_SEARCH:           handleSearchTouch(tx, ty, touched);       break;
        case SCREEN_RESULTS:          handleResultsTouch(tx, ty, touched);      break;
        case SCREEN_PLAYER:           handlePlayerTouch(tx, ty, touched);       break;
        case SCREEN_TRANSLATE:        handleTranslateTouch(tx, ty, touched);    break;
        case SCREEN_TRANSLATE_RESULT: handleTransResultTouch(tx, ty, touched);  break;
    }

    delay(10);
}
