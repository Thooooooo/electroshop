/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   CYD SSH TERMINAL — ESP32 WROOM + ILI9488 3.5"            ║
 * ║   LVGL 9.x  ·  TFT_eSPI touch  ·  LibSSH-ESP32            ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  Display : ILI9488  480×320 landscape  (TFT_eSPI)           ║
 * ║  Touch   : XPT2046 via TFT_eSPI built-in (calData)         ║
 * ║  SSH     : LibSSH-ESP32 → Raspberry Pi                     ║
 * ║  Web     : HTTPClient → electroshop-ten.vercel.app          ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  FQBN: esp32:esp32:esp32:PartitionScheme=huge_app           ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

/* LibSSH headers MUST come first */
#include <arpa/inet.h>
#include "esp_netif.h"
#include "libssh_esp32.h"
#include <libssh/libssh.h>

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/* ══════════════════════════════════════════════════════════
   CREDENTIALS
   ══════════════════════════════════════════════════════════ */
#define WIFI_SSID  "CAFE 669"
#define WIFI_PASS  "51268989"
#define SSH_HOST   "192.168.1.2"
#define SSH_PORT   22
#define SSH_USER   "tho"
#define SSH_PASS   "512689"
#define SHOP_URL   "https://electroshop-ten.vercel.app"

/* ── Screen layout ────────────────────────────────────── */
#define SCR_W    480
#define SCR_H    320
#define BAR_H     28   /* command bar height              */
#define KB_H     200   /* keyboard height  (5 rows×40px)  */
#define HIST_H   (SCR_H - BAR_H - KB_H)   /* = 92px */

/* ── Keyboard button maps (3 modes, ASCII-only font) ─────── */
static const char * const map_abc[] = {          /* MODE 0 — letters */
    "q","w","e","r","t","y","u","i","o","p","BS","\n",
    "a","s","d","f","g","h","j","k","l",";","OK","\n",
    "SHF","z","x","c","v","b","n","m",".","/","\n",
    "123","SFX"," ","-","@",NULL
};
static const char * const map_num[] = {          /* MODE 1 — numbers */
    "1","2","3","4","5","6","7","8","9","0","BS","\n",
    "!","@","#","$","%","^","&","*","(",")", "OK","\n",
    "-","+","=","[","]","{","}","|",":",";","\n",
    "ABC","SFX"," ","~","_",NULL
};
static const char * const map_sfx[] = {          /* MODE 2 — special */
    "UP","DN","LT","RT","TAB","ESC","BS","\n",
    "C-C","C-D","C-Z","C-L","C-A","C-E","OK","\n",
    "C-U","C-W","C-R","|","&",";","~","\n",
    "ABC","123"," ",">","<","`",NULL
};
static uint8_t kb_mode  = 0;
static bool    shift_on = false;

/* ── LVGL draw buffers (heap-allocated to avoid BSS overflow) ── */
#define BUF_ROWS 40
static lv_color_t *buf_a = nullptr;
static lv_color_t *buf_b = nullptr;

/* ── Hardware ─────────────────────────────────────────── */
static TFT_eSPI tft = TFT_eSPI();

/* Touch calibration data for XPT2046 via TFT_eSPI built-in */
static uint16_t calData[5] = {275, 3620, 264, 3532, 3};

/* ── LVGL UI objects ──────────────────────────────────── */
static lv_obj_t *ta_hist = nullptr;
static lv_obj_t *ta_cmd  = nullptr;
static lv_obj_t *kb      = nullptr;
static bool      kb_open = true;

/* ── Terminal colour palette ──────────────────────────── */
#define C_BG      lv_color_hex(0x000000)
#define C_GREEN   lv_color_hex(0x00FF00)
#define C_DKGREEN lv_color_hex(0x003300)
#define C_BAR     lv_color_hex(0x001400)

/* ── SSH task inter-thread messaging ──────────────────── */
static QueueHandle_t q_cmd;
static QueueHandle_t q_result;

struct SshMsg {
    char text[2048];
};

/* ══════════════════════════════════════════════════════════
   1.  LVGL flush → ILI9488
   ══════════════════════════════════════════════════════════ */
static void lv_flush_cb(lv_display_t *disp,
                        const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

/* ══════════════════════════════════════════════════════════
   2.  LVGL touch read — TFT_eSPI built-in XPT2046
   ══════════════════════════════════════════════════════════ */
static void my_touch(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
        data->point.x = tx;
        data->point.y = ty;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ══════════════════════════════════════════════════════════
   3.  Terminal helpers
   ══════════════════════════════════════════════════════════ */
#define TA_MAX 8192

static void term_append(const char *txt)
{
    if (!ta_hist) return;
    const char *cur = lv_textarea_get_text(ta_hist);
    if ((int)(strlen(cur) + strlen(txt)) > TA_MAX) {
        String keep = String(cur).substring(TA_MAX / 2);
        lv_textarea_set_text(ta_hist, keep.c_str());
    }
    lv_textarea_add_text(ta_hist, txt);
    lv_obj_scroll_to_y(ta_hist, LV_COORD_MAX, LV_ANIM_ON);
}

static void term_printf(const char *fmt, ...)
{
    char buf[512]; va_list ap;
    va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    term_append(buf);
}

/* ══════════════════════════════════════════════════════════
   4.  SSH FreeRTOS task (core 0, 52 KB stack)
   ══════════════════════════════════════════════════════════ */
static void ssh_task(void *pv)
{
    libssh_begin();

    ssh_session session = nullptr;
    bool        ok      = false;
    SshMsg      msg;

    auto do_connect = [&]() -> bool {
        if (session) { ssh_disconnect(session); ssh_free(session); }
        session = ssh_new();
        if (!session) return false;
        int port = SSH_PORT, tmo = 12;
        ssh_options_set(session, SSH_OPTIONS_HOST,    SSH_HOST);
        ssh_options_set(session, SSH_OPTIONS_PORT,    &port);
        ssh_options_set(session, SSH_OPTIONS_USER,    SSH_USER);
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &tmo);
        if (ssh_connect(session) != SSH_OK) {
            ssh_free(session); session = nullptr; return false;
        }
        if (ssh_userauth_password(session, nullptr, SSH_PASS) != SSH_AUTH_SUCCESS) {
            ssh_disconnect(session); ssh_free(session); session = nullptr; return false;
        }
        return true;
    };

    while (true) {
        if (xQueueReceive(q_cmd, &msg, portMAX_DELAY) != pdTRUE) continue;

        if (strcmp(msg.text, "__CONNECT__") == 0) {
            ok = do_connect();
            snprintf(msg.text, sizeof msg.text,
                     ok ? "[SSH] Connected to %s\n"
                        : "[SSH] Connection failed!\n", SSH_HOST);
            xQueueSend(q_result, &msg, portMAX_DELAY);
            continue;
        }

        if (!ok) {
            ok = do_connect();
            if (!ok) {
                snprintf(msg.text, sizeof msg.text, "[SSH] Cannot connect to %s\n", SSH_HOST);
                xQueueSend(q_result, &msg, portMAX_DELAY);
                continue;
            }
        }

        ssh_channel ch = ssh_channel_new(session);
        if (!ch || ssh_channel_open_session(ch) != SSH_OK) {
            ok = false;
            snprintf(msg.text, sizeof msg.text, "[SSH] Channel error\n");
            xQueueSend(q_result, &msg, portMAX_DELAY);
            if (ch) ssh_channel_free(ch);
            continue;
        }

        String cmd_str = String(msg.text);
        if (ssh_channel_request_exec(ch, cmd_str.c_str()) != SSH_OK) {
            snprintf(msg.text, sizeof msg.text, "[SSH] Exec error\n");
        } else {
            String out;
            char   rbuf[512]; int n;
            while ((n = ssh_channel_read_timeout(ch, rbuf, sizeof rbuf - 1, 0, 10000)) > 0) {
                rbuf[n] = '\0'; out += rbuf;
                if ((int)out.length() > 3800) { out += "\n...(truncated)\n"; break; }
            }
            while ((n = ssh_channel_read_timeout(ch, rbuf, sizeof rbuf - 1, 1, 200)) > 0) {
                rbuf[n] = '\0'; out += rbuf;
            }
            if (out.isEmpty()) out = "(no output)\n";
            else if (!out.endsWith("\n")) out += "\n";
            snprintf(msg.text, sizeof msg.text, "%s", out.c_str());
        }

        ssh_channel_send_eof(ch);
        ssh_channel_close(ch);
        ssh_channel_free(ch);
        xQueueSend(q_result, &msg, portMAX_DELAY);
    }
}

/* ══════════════════════════════════════════════════════════
   5.  Send command to SSH task (non-blocking)
   ══════════════════════════════════════════════════════════ */
static void send_ssh(const char *cmd)
{
    SshMsg msg;
    strncpy(msg.text, cmd, sizeof msg.text - 1);
    msg.text[sizeof msg.text - 1] = '\0';
    xQueueSend(q_cmd, &msg, 0);
}

/* ══════════════════════════════════════════════════════════
   6.  Fetch ElectroShop products
   ══════════════════════════════════════════════════════════ */
static void fetch_shop()
{
    if (WiFi.status() != WL_CONNECTED) { term_append("[shop] No WiFi\n"); return; }
    term_append("[shop] Loading products...\n");

    HTTPClient http;
    http.begin(SHOP_URL "/api/products?perPage=10&sort=-created");
    http.setTimeout(8000);
    int code = http.GET();
    if (code != 200) { term_printf("[shop] HTTP %d\n", code); http.end(); return; }

    String body = http.getString(); http.end();
    StaticJsonDocument<4096> doc;
    if (deserializeJson(doc, body)) { term_append("[shop] JSON error\n"); return; }

    int total = doc["totalItems"] | 0;
    term_printf("[shop] -- ElectroShop (%d items) --\n", total);
    for (JsonObject item : doc["items"].as<JsonArray>()) {
        const char *name  = item["ten"]  | "?";
        long        price = item["gia"]  | 0;
        const char *type  = item["loai"] | "?";
        term_printf("  %-24s %9ldd [%s]\n", name, price, type);
    }
    term_append("----------------------------------\n");
}

/* ══════════════════════════════════════════════════════════
   6b. Web check — ping key endpoints
   ══════════════════════════════════════════════════════════ */
static void web_check()
{
    if (WiFi.status() != WL_CONNECTED) { term_append("[web] No WiFi\n"); return; }
    term_append("\n-- Web Check --------------------\n");

    struct { const char *label; const char *url; } checks[] = {
        { "Vercel", SHOP_URL "/" },
        { "API   ", SHOP_URL "/api/products?perPage=1" },
        { "PB LAN", "http://" SSH_HOST ":8090/api/health" },
        { "Nginx ", "http://" SSH_HOST ":8080/" },
    };

    HTTPClient http;
    for (auto &c : checks) {
        http.begin(c.url);
        http.setTimeout(6000);
        unsigned long t0 = millis();
        int code = http.GET();
        unsigned long ms = millis() - t0;
        if (code > 0)
            term_printf("  %s HTTP %d  (%lums)\n", c.label, code, ms);
        else
            term_printf("  %s ERR %d\n", c.label, code);
        http.end();
        lv_timer_handler();
    }
    term_append("---------------------------------\n");
}

/* ══════════════════════════════════════════════════════════
   7.  Handle user command
   ══════════════════════════════════════════════════════════ */
static void run_cmd(const String &raw)
{
    String cmd = raw; cmd.trim();
    if (cmd.isEmpty()) return;

    term_printf("\n%s@pi:~$ %s\n", SSH_USER, cmd.c_str());

    if (cmd == "clear" || cmd == "cls") { lv_textarea_set_text(ta_hist, ""); return; }
    if (cmd == "shop")  { fetch_shop(); return; }
    if (cmd == "web")   { web_check();  return; }
    if (cmd == "ip") {
        term_printf("WiFi: %s  SSH: %s\n",
                    WiFi.localIP().toString().c_str(), SSH_HOST);
        return;
    }
    if (cmd == "help") {
        term_append(
            "Commands:\n"
            "  clear/cls  - clear screen\n"
            "  shop       - ElectroShop products\n"
            "  web        - check web endpoints\n"
            "  ip         - show WiFi + SSH host\n"
            "  help       - this help\n"
            "  (other)    - sent via SSH\n"
        );
        return;
    }
    send_ssh(cmd.c_str());
    term_append("...\n");
}

/* ══════════════════════════════════════════════════════════
   8.  Button callbacks
   ══════════════════════════════════════════════════════════ */
static void btn_web_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) web_check();
}

static void btn_kb_cb(lv_event_t *e) {
    kb_open = !kb_open;
    if (kb_open) {
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    /* Append status so user knows button worked */
    term_append(kb_open ? "[KB] shown\n" : "[KB] hidden\n");
}

/* Apply relative widths after setting a map */
static void apply_kb_widths() {
    if (kb_mode == 0) {                              /* ABC */
        lv_btnmatrix_set_btn_width(kb, 10, 2);       /* BS  */
        lv_btnmatrix_set_btn_width(kb, 21, 2);       /* OK  */
        lv_btnmatrix_set_btn_width(kb, 22, 2);       /* SHF */
        lv_btnmatrix_set_btn_width(kb, 32, 2);       /* 123 */
        lv_btnmatrix_set_btn_width(kb, 33, 2);       /* SFX */
        lv_btnmatrix_set_btn_width(kb, 34, 7);       /* SPC */
    } else if (kb_mode == 1) {                       /* NUM */
        lv_btnmatrix_set_btn_width(kb, 10, 2);       /* BS  */
        lv_btnmatrix_set_btn_width(kb, 21, 2);       /* OK  */
        lv_btnmatrix_set_btn_width(kb, 32, 2);       /* ABC */
        lv_btnmatrix_set_btn_width(kb, 33, 2);       /* SFX */
        lv_btnmatrix_set_btn_width(kb, 34, 7);       /* SPC */
    } else {                                         /* SFX */
        lv_btnmatrix_set_btn_width(kb,  6, 2);       /* BS  */
        lv_btnmatrix_set_btn_width(kb, 13, 2);       /* OK  */
        lv_btnmatrix_set_btn_width(kb, 21, 2);       /* ABC */
        lv_btnmatrix_set_btn_width(kb, 22, 2);       /* 123 */
        lv_btnmatrix_set_btn_width(kb, 23, 7);       /* SPC */
    }
}

static void set_kb_mode(uint8_t m) {
    kb_mode  = m;
    shift_on = false;
    const char * const *maps[] = {map_abc, map_num, map_sfx};
    lv_btnmatrix_set_map(kb, maps[m]);
    apply_kb_widths();
}

/* Handles all button-matrix key presses */
static void btnmat_cb(lv_event_t *e) {
    lv_obj_t  *bm  = (lv_obj_t*)lv_event_get_target(e);
    uint32_t   idx = lv_btnmatrix_get_selected_btn(bm);
    const char *t  = lv_btnmatrix_get_btn_text(bm, idx);
    if (!t) return;

    /* ── Mode switches ── */
    if (!strcmp(t,"123"))  { set_kb_mode(1); return; }
    if (!strcmp(t,"SFX"))  { set_kb_mode(2); return; }
    if (!strcmp(t,"ABC"))  { set_kb_mode(0); return; }

    /* ── Shift toggle ── */
    if (!strcmp(t,"SHF")) { shift_on = !shift_on; return; }

    /* ── Backspace / Enter ── */
    if (!strcmp(t,"BS"))  { lv_textarea_delete_char(ta_cmd); return; }
    if (!strcmp(t,"OK"))  {
        String cmd = String(lv_textarea_get_text(ta_cmd));
        lv_textarea_set_text(ta_cmd, "");
        run_cmd(cmd);
        return;
    }

    /* ── Navigation (scroll history / move cursor) ── */
    if (!strcmp(t,"UP"))  { lv_obj_scroll_by(ta_hist, 0,  28, LV_ANIM_OFF); return; }
    if (!strcmp(t,"DN"))  { lv_obj_scroll_by(ta_hist, 0, -28, LV_ANIM_OFF); return; }
    if (!strcmp(t,"LT"))  { lv_textarea_cursor_left(ta_cmd);  return; }
    if (!strcmp(t,"RT"))  { lv_textarea_cursor_right(ta_cmd); return; }

    /* ── Special keys ── */
    if (!strcmp(t,"TAB")) { lv_textarea_add_text(ta_cmd, "\t"); return; }
    if (!strcmp(t,"ESC")) { lv_textarea_add_text(ta_cmd, "\x1b"); return; }

    /* ── Ctrl sequences ── */
    if (!strcmp(t,"C-C")) { lv_textarea_add_text(ta_cmd, "\x03"); return; }
    if (!strcmp(t,"C-D")) { lv_textarea_add_text(ta_cmd, "\x04"); return; }
    if (!strcmp(t,"C-Z")) { lv_textarea_add_text(ta_cmd, "\x1a"); return; }
    if (!strcmp(t,"C-L")) { lv_textarea_set_text(ta_hist, "[screen cleared]\n"); return; }
    if (!strcmp(t,"C-A")) { lv_textarea_add_text(ta_cmd, "\x01"); return; }
    if (!strcmp(t,"C-E")) { lv_textarea_add_text(ta_cmd, "\x05"); return; }
    if (!strcmp(t,"C-U")) { lv_textarea_set_text(ta_cmd, "");     return; }
    if (!strcmp(t,"C-W")) { lv_textarea_add_text(ta_cmd, "\x17"); return; }
    if (!strcmp(t,"C-R")) { lv_textarea_add_text(ta_cmd, "\x12"); return; }

    /* ── Regular char (with optional shift) ── */
    if (shift_on && strlen(t)==1 && t[0]>='a' && t[0]<='z') {
        char up[2] = { (char)(t[0]-32), '\0' };
        lv_textarea_add_text(ta_cmd, up);
        shift_on = false;
    } else {
        lv_textarea_add_text(ta_cmd, t);
    }
}

/* ══════════════════════════════════════════════════════════
   9.  Build UI
   ══════════════════════════════════════════════════════════ */
static void build_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa (scr, LV_OPA_COVER, 0);

    /* Terminal history style */
    static lv_style_t st_term;
    lv_style_init(&st_term);
    lv_style_set_bg_color    (&st_term, C_BG);
    lv_style_set_bg_opa      (&st_term, LV_OPA_COVER);
    lv_style_set_text_color  (&st_term, C_GREEN);
    lv_style_set_text_font   (&st_term, &lv_font_unscii_8);
    lv_style_set_border_width(&st_term, 0);
    lv_style_set_radius      (&st_term, 0);
    lv_style_set_pad_all     (&st_term, 4);

    /* ta_hist — y=0, height=HIST_H when kb open */
    ta_hist = lv_textarea_create(scr);
    lv_obj_set_size(ta_hist, SCR_W, HIST_H);
    lv_obj_set_pos (ta_hist, 0, 0);
    lv_obj_add_style(ta_hist, &st_term, 0);
    lv_obj_remove_flag(ta_hist, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_cursor_click_pos(ta_hist, false);
    lv_obj_set_scrollbar_mode(ta_hist, LV_SCROLLBAR_MODE_ACTIVE);
    lv_textarea_set_text(ta_hist,
        "+----------------------------------------+\n"
        "|  CYD SSH Terminal  v3.0                |\n"
        "|  Pi: " SSH_HOST "  [WEB]=check urls    |\n"
        "|  [KB]=toggle keyboard  'help'=commands |\n"
        "+----------------------------------------+\n\n"
    );

    /* Bar styles */
    static lv_style_t st_bar, st_btn_n, st_btn_p;
    lv_style_init(&st_bar);
    lv_style_set_bg_color    (&st_bar, C_BAR);
    lv_style_set_bg_opa      (&st_bar, LV_OPA_COVER);
    lv_style_set_text_color  (&st_bar, lv_color_hex(0xAAFFAA));
    lv_style_set_text_font   (&st_bar, &lv_font_unscii_8);
    lv_style_set_border_color(&st_bar, C_DKGREEN);
    lv_style_set_border_width(&st_bar, 1);
    lv_style_set_border_side (&st_bar, LV_BORDER_SIDE_TOP);
    lv_style_set_radius      (&st_bar, 0);
    lv_style_set_pad_all     (&st_bar, 4);

    lv_style_init(&st_btn_n);
    lv_style_set_bg_color    (&st_btn_n, lv_color_hex(0x003300));
    lv_style_set_bg_opa      (&st_btn_n, LV_OPA_COVER);
    lv_style_set_text_color  (&st_btn_n, C_GREEN);
    lv_style_set_text_font   (&st_btn_n, &lv_font_unscii_8);
    lv_style_set_border_color(&st_btn_n, C_DKGREEN);
    lv_style_set_border_width(&st_btn_n, 1);
    lv_style_set_radius      (&st_btn_n, 3);
    lv_style_set_pad_all     (&st_btn_n, 0);

    lv_style_init(&st_btn_p);
    lv_style_set_bg_color(&st_btn_p, lv_color_hex(0x00AA00));

    /* ta_cmd — y=HIST_H, width=372 */
    ta_cmd = lv_textarea_create(scr);
    lv_obj_set_size(ta_cmd, 372, BAR_H);
    lv_obj_set_pos (ta_cmd, 0, HIST_H);
    lv_obj_add_style(ta_cmd, &st_bar, 0);
    lv_textarea_set_one_line(ta_cmd, true);
    lv_textarea_set_placeholder_text(ta_cmd, SSH_USER "@pi:~$ ");
    /* Force green text + cursor visible regardless of focus state */
    lv_obj_set_style_text_color(ta_cmd, lv_color_hex(0x00FF00), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ta_cmd, lv_color_hex(0x00FF00), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color  (ta_cmd, lv_color_hex(0x001400), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color  (ta_cmd, lv_color_hex(0x002800), LV_STATE_FOCUSED);

    /* [WEB] button — x=372, y=HIST_H */
    lv_obj_t *btn_web = lv_button_create(scr);
    lv_obj_set_size(btn_web, 52, BAR_H);
    lv_obj_set_pos (btn_web, 372, HIST_H);
    lv_obj_add_style(btn_web, &st_btn_n, 0);
    lv_obj_add_style(btn_web, &st_btn_p, LV_STATE_PRESSED);
    lv_obj_remove_flag(btn_web, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(btn_web, btn_web_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(btn_web); lv_label_set_text(l, "WEB"); lv_obj_center(l); }

    /* [KB] button — x=426, y=HIST_H */
    lv_obj_t *btn_kb = lv_button_create(scr);
    lv_obj_set_size(btn_kb, 52, BAR_H);
    lv_obj_set_pos (btn_kb, 426, HIST_H);
    lv_obj_add_style(btn_kb, &st_btn_n, 0);
    lv_obj_add_style(btn_kb, &st_btn_p, LV_STATE_PRESSED);
    lv_obj_remove_flag(btn_kb, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(btn_kb, btn_kb_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(btn_kb); lv_label_set_text(l, "KB"); lv_obj_center(l); }

    /* Keyboard styles */
    static lv_style_t st_kb, st_key;
    lv_style_init(&st_kb);
    lv_style_set_bg_color    (&st_kb, lv_color_hex(0x001900));
    lv_style_set_bg_opa      (&st_kb, LV_OPA_COVER);
    lv_style_set_border_width(&st_kb, 0);
    lv_style_set_radius      (&st_kb, 0);
    lv_style_set_pad_all     (&st_kb, 2);   /* minimal outer padding */
    lv_style_set_pad_row     (&st_kb, 2);   /* gap between key rows  */
    lv_style_set_pad_column  (&st_kb, 2);   /* gap between keys      */

    lv_style_init(&st_key);
    lv_style_set_bg_color    (&st_key, lv_color_hex(0x003300));
    lv_style_set_text_color  (&st_key, C_GREEN);
    lv_style_set_text_font   (&st_key, &lv_font_unscii_8);
    lv_style_set_border_color(&st_key, C_DKGREEN);
    lv_style_set_border_width(&st_key, 1);
    lv_style_set_radius      (&st_key, 3);
    lv_style_set_pad_all     (&st_key, 4);  /* key inner padding     */

    /* ── Keyboard (btnmatrix — full position control, no auto-hide) ── */
    kb = lv_btnmatrix_create(scr);
    lv_btnmatrix_set_map(kb, map_abc);
    lv_obj_set_size(kb, SCR_W, KB_H);
    lv_obj_set_pos (kb, 0, HIST_H + BAR_H);   /* absolute pos — btnmatrix respects it */
    lv_obj_add_style(kb, &st_kb,  0);
    lv_obj_add_style(kb, &st_key, LV_PART_ITEMS);
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(kb, btnmat_cb, LV_EVENT_CLICKED, nullptr);
    apply_kb_widths();

    kb_open = true;
}

/* ══════════════════════════════════════════════════════════
   setup()
   ══════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);

    /* Backlight */
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    /* ILI9488 landscape 480×320 */
    tft.init();
    tft.setRotation(1);
    tft.setTouch(calData);
    Serial.println("[ok] TFT + touch init");

    /* Allocate LVGL buffers — prefer DMA, fallback to regular heap */
    size_t bsz = (size_t)SCR_W * BUF_ROWS * sizeof(lv_color_t);
    buf_a = (lv_color_t*)heap_caps_malloc(bsz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf_b = (lv_color_t*)heap_caps_malloc(bsz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf_a) buf_a = (lv_color_t*)malloc(bsz);
    if (!buf_b) buf_b = (lv_color_t*)malloc(bsz);
    Serial.printf("[mem] buf_a=%p buf_b=%p size=%u\n", buf_a, buf_b, bsz);

    /* LVGL 9 init */
    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)millis);

    lv_display_t *disp = lv_display_create(SCR_W, SCR_H);
    lv_display_set_flush_cb(disp, lv_flush_cb);
    /* Use bsz — NOT sizeof(buf_a) which is just a pointer (4 bytes) */
    lv_display_set_buffers(disp, buf_a, buf_b, bsz, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type   (indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch);

    /* Build UI */
    Serial.println("[ok] LVGL init, building UI");
    build_ui();
    lv_timer_handler();
    Serial.println("[ok] UI ready");

    /* FreeRTOS queues for SSH */
    q_cmd    = xQueueCreate(4, sizeof(SshMsg));
    q_result = xQueueCreate(4, sizeof(SshMsg));

    /* SSH task — core 0, 52 KB stack */
    xTaskCreatePinnedToCore(ssh_task, "ssh", 53248, nullptr, 1, nullptr, 0);

    /* Connect WiFi */
    term_printf("Connecting WiFi: %s\n", WIFI_SSID);
    lv_timer_handler();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500); term_append("."); lv_timer_handler();
    }
    if (WiFi.status() == WL_CONNECTED)
        term_printf("\nWiFi OK: %s\n\n", WiFi.localIP().toString().c_str());
    else
        term_append("\n[!] WiFi failed\n\n");

    /* Auto-connect SSH on boot */
    send_ssh("__CONNECT__");
}

/* ══════════════════════════════════════════════════════════
   loop()
   ══════════════════════════════════════════════════════════ */
void loop()
{
    lv_timer_handler();

    /* Force keyboard visible if LVGL's class handler auto-hid it */
    if (kb != nullptr) {
        if (kb_open && lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
        } else if (!kb_open && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Always drain ALL pending SSH results */
    {
        SshMsg msg;
        while (xQueueReceive(q_result, &msg, 0) == pdTRUE) {
            term_append(msg.text);
        }
    }
}
