/*
 * TOUCH TEST — CYD 3.5" ESP32 WROOM
 * Dùng TFT_eSPI built-in touch (tft.setTouch / tft.getTouch)
 * KHÔNG dùng XPT2046_Touchscreen library
 */
#include <TFT_eSPI.h>

#define TFT_BL 27

// Calibration data đo thực tế từ board này
uint16_t calData[5] = {275, 3620, 264, 3532, 3};

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Kích hoạt touch tích hợp
    tft.setTouch(calData);

    // Hướng dẫn
    tft.setTextColor(TFT_CYAN);
    tft.drawString("TOUCH TEST", 160, 20, 4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Cham vao man hinh bat ky dau", 60, 70, 2);

    // Lưới tham chiếu
    tft.drawRect(0, 0, 480, 320, TFT_DARKGREY);
    tft.drawLine(240, 0, 240, 320, TFT_DARKGREY);
    tft.drawLine(0, 160, 480, 160, TFT_DARKGREY);

    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("0,0",     4,   4,   1);
    tft.drawString("479,0",   440, 4,   1);
    tft.drawString("0,319",   4,   310, 1);
    tft.drawString("479,319", 425, 310, 1);

    Serial.println("Touch test ready — dung TFT_eSPI built-in");
}

int lastX = -1, lastY = -1;

void loop() {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty, 1200)) {

        // Xóa chấm cũ
        if (lastX >= 0) {
            tft.fillCircle(lastX, lastY, 8, TFT_BLACK);
            // Vẽ lại lưới nếu chấm đè lên
            if (lastX > 235 && lastX < 245)
                tft.drawLine(240, 0, 240, 320, TFT_DARKGREY);
            if (lastY > 155 && lastY < 165)
                tft.drawLine(0, 160, 480, 160, TFT_DARKGREY);
        }

        // Vẽ chấm đỏ
        tft.fillCircle(tx, ty, 8, TFT_RED);
        tft.drawCircle(tx, ty, 9, TFT_WHITE);

        // Xóa text cũ
        tft.fillRect(0, 95, 480, 55, TFT_BLACK);

        // Hiện tọa độ
        char buf[50];
        tft.setTextColor(TFT_GREEN);
        sprintf(buf, "x = %d", tx);
        tft.drawString(buf, 20, 100, 4);
        tft.setTextColor(TFT_YELLOW);
        sprintf(buf, "y = %d", ty);
        tft.drawString(buf, 20, 130, 4);

        Serial.printf("Touch: x=%d  y=%d\n", tx, ty);

        lastX = tx;
        lastY = ty;
        delay(30);
    }
}
