#pragma once
#include <WiFi.h>
#include <Preferences.h>

extern TFT_eSPI tft;

// Defined in web_editor.h, included after this file
void startWebServer();
void stopWebServer();

extern bool needRedraw;

// --- State machine -----------------------------------------------------------
enum WifiPageState { WPS_STATUS, WPS_SCANNING, WPS_KEYBOARD, WPS_CONNECTING };

static WifiPageState  wpsState        = WPS_STATUS;
static String         wifiConnSSID    = "";
static String         wifiConnIP      = "";
static String         wifiTargetSSID  = "";
static String         wifiPassword    = "";
static bool           wifiIsConnected = false;
static String         scannedSSIDs[8];
static int32_t        scannedRSSI[8];
static int            scannedCount    = 0;
static unsigned long  connectStartMs  = 0;
static int            connectDots     = 0;
static unsigned long  lastDotMs       = 0;

// Layout positions set during draw, consumed during touch
static int statusBtnY     = 0;
static int scanListStartY = 0;
static const int pwFieldY = CONTENT_Y + 30;   // = 75
static const int kbStartY = CONTENT_Y + 62;   // = 107

// Non-blocking fail-message dismissal
static unsigned long wpsFailUntilMs = 0;

// --- NVS helpers -------------------------------------------------------------
static void wifiSaveCreds(const String& ssid, const String& pass) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

static void wifiLoadCreds(String& ssid, String& pass) {
    Preferences prefs;
    prefs.begin("wifi", true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    prefs.end();
}

// --- WiFi event handler ------------------------------------------------------
static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        wifiIsConnected = true;
        wifiConnSSID    = WiFi.SSID();
        wifiConnIP      = WiFi.localIP().toString();
        wpsState        = WPS_STATUS;
        startWebServer();
        needRedraw = true;
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        wifiIsConnected = false;
        wifiConnSSID    = "";
        wifiConnIP      = "";
        stopWebServer();
    }
}

// --- initWifi ----------------------------------------------------------------
void initWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(wifiEventHandler);
    String savedSSID, savedPass;
    wifiLoadCreds(savedSSID, savedPass);
    if (savedSSID.length() > 0) {
        wifiTargetSSID = savedSSID;
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        wpsState       = WPS_CONNECTING;
        connectStartMs = millis();
        connectDots    = 0;
        lastDotMs      = millis();
    }
}

// --- Internal helpers --------------------------------------------------------

static void drawSignalBars(int x, int y, int32_t rssi) {
    int bars = 0;
    if      (rssi >= -55) bars = 4;
    else if (rssi >= -66) bars = 3;
    else if (rssi >= -77) bars = 2;
    else if (rssi >= -88) bars = 1;
    for (int b = 0; b < 4; b++) {
        int bx = x + b * 5;
        int bh = (b + 1) * 4;
        int by = y + 16 - bh;
        tft.fillRect(bx, by, 4, bh, (b < bars) ? C_GREEN : C_GRAY);
    }
}

static void redrawPasswordField() {
    tft.fillRect(8, pwFieldY, 464, 28, C_HEADER);
    tft.drawRect(8, pwFieldY, 464, 28, C_CYAN);
    tft.setTextSize(2);
    if (wifiPassword.length() > 0) {
        tft.setTextColor(C_WHITE, C_HEADER);
        String stars = "";
        int showLen = min((int)wifiPassword.length(), 29);
        for (int i = 0; i < showLen; i++) stars += '*';
        tft.setCursor(14, pwFieldY + 6);
        tft.print(stars);
    } else {
        tft.setTextColor(C_GRAY, C_HEADER);
        tft.setCursor(14, pwFieldY + 6);
        tft.print("Enter password...");
    }
}

// --- drawPageWifi ------------------------------------------------------------
void drawPageWifi() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, C_BG);

    // -- WPS_STATUS -----------------------------------------------------------
    if (wpsState == WPS_STATUS) {
        tft.setTextColor(C_CYAN, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 8);
        tft.print("Wi-Fi MANAGER");

        tft.drawFastHLine(0, CONTENT_Y + 28, SCREEN_W, C_GRAY);

        int cardY = CONTENT_Y + 36;
        if (wifiIsConnected) {
            tft.drawRect(4, cardY, SCREEN_W - 8, 62, C_GREEN);
            tft.drawRect(5, cardY + 1, SCREEN_W - 10, 60, C_DKGREEN);
            tft.setTextColor(C_GREEN, C_BG);
            tft.setTextSize(2);
            tft.setCursor(12, cardY + 6);
            tft.print("CONNECTED");
            tft.setTextColor(C_WHITE, C_BG);
            tft.setTextSize(1);
            tft.setCursor(12, cardY + 26);
            tft.print("SSID: ");
            tft.print(wifiConnSSID);
            tft.setCursor(12, cardY + 40);
            tft.print("IP:   ");
            tft.print(wifiConnIP);
        } else {
            tft.drawRect(4, cardY, SCREEN_W - 8, 62, C_RED);
            tft.drawRect(5, cardY + 1, SCREEN_W - 10, 60, C_DKRED);
            tft.setTextColor(C_RED, C_BG);
            tft.setTextSize(2);
            tft.setCursor(12, cardY + 6);
            tft.print("DISCONNECTED");
            tft.setTextColor(C_LGRAY, C_BG);
            tft.setTextSize(1);
            tft.setCursor(12, cardY + 30);
            tft.print("Tap SCAN to find networks");
        }

        statusBtnY = cardY + 70;

        // SCAN button  x=8, w=130
        tft.fillRect(8, statusBtnY, 130, 38, C_DKBLUE);
        tft.drawRect(8, statusBtnY, 130, 38, C_CYAN);
        tft.setTextColor(C_CYAN, C_DKBLUE);
        tft.setTextSize(2);
        tft.setCursor(28, statusBtnY + 11);
        tft.print("SCAN");

        if (wifiIsConnected) {
            tft.fillRect(148, statusBtnY, 180, 38, C_DKRED);
            tft.drawRect(148, statusBtnY, 180, 38, C_RED);
            tft.setTextColor(C_RED, C_DKRED);
            tft.setTextSize(2);
            tft.setCursor(155, statusBtnY + 11);
            tft.print("DISCONNECT");
        } else {
            String savedSSID, savedPass;
            wifiLoadCreds(savedSSID, savedPass);
            if (savedSSID.length() > 0) {
                tft.fillRect(148, statusBtnY, 180, 38, C_DKBLUE);
                tft.drawRect(148, statusBtnY, 180, 38, C_BLUE);
                tft.setTextColor(C_WHITE, C_DKBLUE);
                tft.setTextSize(2);
                tft.setCursor(153, statusBtnY + 11);
                tft.print("RECONNECT");
            }
        }

    // -- WPS_SCANNING ---------------------------------------------------------
    } else if (wpsState == WPS_SCANNING) {
        tft.setTextColor(C_YELLOW, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 8);
        tft.print("SELECT NETWORK");

        tft.drawFastHLine(0, CONTENT_Y + 28, SCREEN_W, C_GRAY);

        scanListStartY  = CONTENT_Y + 36;
        const int itemH = 34;
        int maxItems    = min(scannedCount, 6);

        for (int i = 0; i < maxItems; i++) {
            int      iy    = scanListStartY + i * itemH;
            uint16_t bgCol = (i % 2 == 0) ? C_HEADER : C_BG;
            if (wifiIsConnected && scannedSSIDs[i] == wifiConnSSID)
                bgCol = C_DKGREEN;

            tft.fillRect(0, iy, SCREEN_W, itemH - 2, bgCol);

            String ssidDisp = scannedSSIDs[i];
            if (ssidDisp.length() > 34) ssidDisp = ssidDisp.substring(0, 34);
            tft.setTextColor(C_WHITE, bgCol);
            tft.setTextSize(1);
            tft.setCursor(6, iy + 12);
            tft.print(ssidDisp);

            tft.setTextColor(C_LGRAY, bgCol);
            tft.setCursor(SCREEN_W - 80, iy + 12);
            tft.print(String(scannedRSSI[i]) + "dBm");

            drawSignalBars(SCREEN_W - 26, iy + 8, scannedRSSI[i]);
        }

        if (scannedCount == 0) {
            tft.setTextColor(C_GRAY, C_BG);
            tft.setTextSize(1);
            tft.setCursor(8, scanListStartY + 16);
            tft.print("No networks found.");
        }

        int backBtnY = CONTENT_Y + CONTENT_H - 26;
        tft.fillRect(4, backBtnY, 80, 22, C_HEADER);
        tft.drawRect(4, backBtnY, 80, 22, C_GRAY);
        tft.setTextColor(C_WHITE, C_HEADER);
        tft.setTextSize(1);
        tft.setCursor(10, backBtnY + 7);
        tft.print("< BACK");

    // -- WPS_KEYBOARD ---------------------------------------------------------
    } else if (wpsState == WPS_KEYBOARD) {
        // Row 1: back button + SSID label
        tft.fillRect(2, CONTENT_Y + 4, 60, 22, C_HEADER);
        tft.drawRect(2, CONTENT_Y + 4, 60, 22, C_GRAY);
        tft.setTextColor(C_WHITE, C_HEADER);
        tft.setTextSize(1);
        tft.setCursor(8, CONTENT_Y + 11);
        tft.print("< BACK");

        tft.setTextColor(C_CYAN, C_BG);
        tft.setCursor(70, CONTENT_Y + 11);
        tft.print("SSID: ");
        tft.setTextColor(C_WHITE, C_BG);
        String ssidDisp = wifiTargetSSID;
        if (ssidDisp.length() > 30) ssidDisp = ssidDisp.substring(0, 30);
        tft.print(ssidDisp);

        redrawPasswordField();

        static const char row0[] = "QWERTYUIOP";
        static const char row1[] = "ASDFGHJKL";
        static const char row2[] = "ZXCVBNM";
        const int keyW = 44, keyH = 30;

        // Row 0 -- y = kbStartY
        for (int k = 0; k < 10; k++) {
            int kx = 4 + k * 48, ky = kbStartY;
            tft.fillRect(kx, ky, keyW, keyH, C_HEADER);
            tft.drawRect(kx, ky, keyW, keyH, C_GRAY);
            tft.setTextColor(C_WHITE, C_HEADER);
            tft.setTextSize(2);
            tft.setCursor(kx + 14, ky + 7);
            tft.print(row0[k]);
        }

        // Row 1 -- y = kbStartY+34
        for (int k = 0; k < 9; k++) {
            int kx = 28 + k * 48, ky = kbStartY + 34;
            tft.fillRect(kx, ky, keyW, keyH, C_HEADER);
            tft.drawRect(kx, ky, keyW, keyH, C_GRAY);
            tft.setTextColor(C_WHITE, C_HEADER);
            tft.setTextSize(2);
            tft.setCursor(kx + 14, ky + 7);
            tft.print(row1[k]);
        }

        // Row 2 -- y = kbStartY+68
        for (int k = 0; k < 7; k++) {
            int kx = 76 + k * 48, ky = kbStartY + 68;
            tft.fillRect(kx, ky, keyW, keyH, C_HEADER);
            tft.drawRect(kx, ky, keyW, keyH, C_GRAY);
            tft.setTextColor(C_WHITE, C_HEADER);
            tft.setTextSize(2);
            tft.setCursor(kx + 14, ky + 7);
            tft.print(row2[k]);
        }

        // Bottom row -- y = kbStartY+102
        int bRowY = kbStartY + 102;

        // SPACE  x=4, w=180
        tft.fillRect(4, bRowY, 180, keyH, C_HEADER);
        tft.drawRect(4, bRowY, 180, keyH, C_GRAY);
        tft.setTextColor(C_WHITE, C_HEADER);
        tft.setTextSize(2);
        tft.setCursor(62, bRowY + 7);
        tft.print("SPACE");

        // BKSP  x=186, w=160
        tft.fillRect(186, bRowY, 160, keyH, C_HEADER);
        tft.drawRect(186, bRowY, 160, keyH, C_ORANGE);
        tft.setTextColor(C_ORANGE, C_HEADER);
        tft.setTextSize(2);
        tft.setCursor(218, bRowY + 7);
        tft.print("BKSP");

        // CONNECT  x=348, w=128
        tft.fillRect(348, bRowY, 128, keyH, C_DKGREEN);
        tft.drawRect(348, bRowY, 128, keyH, C_GREEN);
        tft.setTextColor(C_GREEN, C_DKGREEN);
        tft.setTextSize(1);
        tft.setCursor(364, bRowY + 11);
        tft.print("CONNECT");

    // -- WPS_CONNECTING -------------------------------------------------------
    } else if (wpsState == WPS_CONNECTING) {
        tft.setTextColor(C_CYAN, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 20);
        tft.print("Connecting to:");

        tft.setTextColor(C_WHITE, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 50);
        String ssidDisp = wifiTargetSSID;
        if (ssidDisp.length() > 22) ssidDisp = ssidDisp.substring(0, 22);
        tft.print(ssidDisp);

        // Dots area (partially updated by wifiTick)
        tft.setTextColor(C_YELLOW, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 90);
        String dots = "";
        for (int d = 0; d < connectDots; d++) dots += '.';
        for (int d = connectDots; d < 4; d++) dots += ' ';
        tft.print(dots);

        // CANCEL  x=8, y=CONTENT_Y+150, w=120, h=38
        tft.fillRect(8, CONTENT_Y + 150, 120, 38, C_DKRED);
        tft.drawRect(8, CONTENT_Y + 150, 120, 38, C_RED);
        tft.setTextColor(C_RED, C_DKRED);
        tft.setTextSize(2);
        tft.setCursor(20, CONTENT_Y + 161);
        tft.print("CANCEL");
    }
}

// --- wifiTick: call every loop() ---------------------------------------------
void wifiTick() {
    unsigned long now = millis();

    // Dismiss fail message after 2 s, then restore STATUS page
    if (wpsFailUntilMs > 0 && now >= wpsFailUntilMs) {
        wpsFailUntilMs = 0;
        drawPageWifi();
        return;
    }

    if (wpsState != WPS_CONNECTING) return;

    // Animate dots every 400 ms (partial redraw only, no delay())
    if (now - lastDotMs >= 400) {
        lastDotMs   = now;
        connectDots = (connectDots + 1) % 5;
        tft.fillRect(8, CONTENT_Y + 90, 100, 20, C_BG);
        tft.setTextColor(C_YELLOW, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 90);
        String dots = "";
        for (int d = 0; d < connectDots; d++) dots += '.';
        tft.print(dots);
    }

    // Timeout after 15 s
    if (now - connectStartMs >= 15000) {
        WiFi.disconnect(true);
        wifiIsConnected = false;
        wpsState        = WPS_STATUS;

        tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, C_BG);
        tft.setTextColor(C_RED, C_BG);
        tft.setTextSize(2);
        tft.setCursor(8, CONTENT_Y + 80);
        tft.print("Connection failed!");
        tft.setTextColor(C_LGRAY, C_BG);
        tft.setTextSize(1);
        tft.setCursor(8, CONTENT_Y + 110);
        tft.print("Check password and try again.");
        wpsFailUntilMs = now + 2000;
    }
}

// --- handleWifiTouch ---------------------------------------------------------
void handleWifiTouch(uint16_t tx, uint16_t ty) {

    // -- STATUS ---------------------------------------------------------------
    if (wpsState == WPS_STATUS) {
        if (ty < (uint16_t)statusBtnY || ty >= (uint16_t)(statusBtnY + 38)) return;

        // SCAN  x=8, w=130
        if (tx >= 8 && tx < 138) {
            wpsState = WPS_SCANNING;
            tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, C_BG);
            tft.setTextColor(C_YELLOW, C_BG);
            tft.setTextSize(2);
            tft.setCursor(8, CONTENT_Y + 80);
            tft.print("Scanning...");

            int n = WiFi.scanNetworks();
            scannedCount = (n > 0) ? min(n, 8) : 0;
            for (int i = 0; i < scannedCount; i++) {
                scannedSSIDs[i] = WiFi.SSID(i);
                scannedRSSI[i]  = WiFi.RSSI(i);
            }
            WiFi.scanDelete();
            drawPageWifi();
            return;
        }

        // DISCONNECT / RECONNECT  x=148, w=180
        if (tx >= 148 && tx < 328) {
            if (wifiIsConnected) {
                WiFi.disconnect(true);
                wifiIsConnected = false;
                wifiConnSSID    = "";
                wifiConnIP      = "";
                wpsState        = WPS_STATUS;
                drawPageWifi();
            } else {
                String savedSSID, savedPass;
                wifiLoadCreds(savedSSID, savedPass);
                if (savedSSID.length() > 0) {
                    wifiTargetSSID = savedSSID;
                    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
                    wpsState       = WPS_CONNECTING;
                    connectStartMs = millis();
                    connectDots    = 0;
                    lastDotMs      = millis();
                    drawPageWifi();
                }
            }
        }

    // -- SCANNING -------------------------------------------------------------
    } else if (wpsState == WPS_SCANNING) {
        int backBtnY = CONTENT_Y + CONTENT_H - 26;

        if (tx >= 4 && tx < 84 && ty >= (uint16_t)backBtnY && ty < (uint16_t)(backBtnY + 22)) {
            wpsState = WPS_STATUS;
            drawPageWifi();
            return;
        }

        const int itemH  = 34;
        int maxItems     = min(scannedCount, 6);
        for (int i = 0; i < maxItems; i++) {
            int iy = scanListStartY + i * itemH;
            if (ty >= (uint16_t)iy && ty < (uint16_t)(iy + itemH - 2)) {
                wifiTargetSSID = scannedSSIDs[i];
                wifiPassword   = "";
                wpsState       = WPS_KEYBOARD;
                drawPageWifi();
                return;
            }
        }

    // -- KEYBOARD -------------------------------------------------------------
    } else if (wpsState == WPS_KEYBOARD) {
        static const char row0[] = "QWERTYUIOP";
        static const char row1[] = "ASDFGHJKL";
        static const char row2[] = "ZXCVBNM";

        // BACK  tx<62, ty in [CONTENT_Y+4, CONTENT_Y+26)
        if (tx < 62 && ty >= (uint16_t)(CONTENT_Y + 4) && ty < (uint16_t)(CONTENT_Y + 26)) {
            wpsState = WPS_SCANNING;
            drawPageWifi();
            return;
        }

        // Row 0: y in [kbStartY, kbStartY+32)
        if (ty >= (uint16_t)kbStartY && ty < (uint16_t)(kbStartY + 32)) {
            for (int k = 0; k < 10; k++) {
                int kx = 4 + k * 48;
                if (tx >= (uint16_t)kx && tx < (uint16_t)(kx + 44)) {
                    if (wifiPassword.length() < 63) wifiPassword += row0[k];
                    redrawPasswordField();
                    return;
                }
            }
        }

        // Row 1: y in [kbStartY+34, kbStartY+66)
        if (ty >= (uint16_t)(kbStartY + 34) && ty < (uint16_t)(kbStartY + 66)) {
            for (int k = 0; k < 9; k++) {
                int kx = 28 + k * 48;
                if (tx >= (uint16_t)kx && tx < (uint16_t)(kx + 44)) {
                    if (wifiPassword.length() < 63) wifiPassword += row1[k];
                    redrawPasswordField();
                    return;
                }
            }
        }

        // Row 2: y in [kbStartY+68, kbStartY+100)
        if (ty >= (uint16_t)(kbStartY + 68) && ty < (uint16_t)(kbStartY + 100)) {
            for (int k = 0; k < 7; k++) {
                int kx = 76 + k * 48;
                if (tx >= (uint16_t)kx && tx < (uint16_t)(kx + 44)) {
                    if (wifiPassword.length() < 63) wifiPassword += row2[k];
                    redrawPasswordField();
                    return;
                }
            }
        }

        // Bottom row: y in [kbStartY+102, kbStartY+134)
        if (ty >= (uint16_t)(kbStartY + 102) && ty < (uint16_t)(kbStartY + 134)) {
            if (tx < 186) {
                if (wifiPassword.length() < 63) wifiPassword += ' ';
                redrawPasswordField();
            } else if (tx < 348) {
                if (wifiPassword.length() > 0)
                    wifiPassword.remove(wifiPassword.length() - 1);
                redrawPasswordField();
            } else {
                wifiSaveCreds(wifiTargetSSID, wifiPassword);
                WiFi.begin(wifiTargetSSID.c_str(), wifiPassword.c_str());
                wpsState       = WPS_CONNECTING;
                connectStartMs = millis();
                connectDots    = 0;
                lastDotMs      = millis();
                drawPageWifi();
            }
        }

    // -- CONNECTING -----------------------------------------------------------
    } else if (wpsState == WPS_CONNECTING) {
        // CANCEL  x=8, w=120, y=CONTENT_Y+150, h=38
        if (tx >= 8 && tx < 128 &&
            ty >= (uint16_t)(CONTENT_Y + 150) && ty < (uint16_t)(CONTENT_Y + 188)) {
            WiFi.disconnect(true);
            wifiIsConnected = false;
            wpsFailUntilMs  = 0;
            wpsState        = WPS_STATUS;
            drawPageWifi();
        }
    }
}
