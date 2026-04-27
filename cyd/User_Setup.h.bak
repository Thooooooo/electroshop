// ═══════════════════════════════════════════════════════════
//  TFT_eSPI User_Setup.h — ESP32-2432S035 (CYD 3.5")
//  ILI9488  480x320  |  XPT2046 touch  |  CH340C serial
// ═══════════════════════════════════════════════════════════

#define ILI9488_DRIVER

// Kích thước vật lý (portrait), rotation=1 trong code = landscape 480x320
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// ── SPI display pins ─────────────────────────────────────
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // Không có chân RST riêng → -1
#define TFT_BL   27   // Backlight (HIGH = ON)

// ── Touch XPT2046 ─────────────────────────────────────────
#define TOUCH_CS 33
// T_CLK=25, T_MOSI=32, T_MISO=39 — TFT_eSPI tự quản lý SPI phụ

// ── Fonts ─────────────────────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ── SPI speed ─────────────────────────────────────────────
#define SPI_FREQUENCY        27000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000
