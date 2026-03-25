#!/usr/bin/env python3
"""
pi_monitor.py — Màn hình debug ST7735 cho ElectroShop trên Raspberry Pi 5
Dùng thư viện st7735 (Pimoroni) + Pillow — giống st7735_logs.py đang hoạt động.

Chạy thử  : python3 pi_monitor.py
Autostart : sudo systemctl start electroshop-monitor

BLACKTAB mode:
  invert=False  → thư viện KHÔNG gửi INVON (0x21)
  bgr=True      → byte order BGR (mặc định đúng cho BLACKTAB)
  disp.command(0x20) sau begin() → ép INVOFF ở phần cứng
  → Kết quả: màu đen = đen, màu trắng = trắng, không bị đảo
"""

import os, re, sys, time, threading, requests
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Hỗ trợ cả 'st7735' (chữ thường) và 'ST7735' (chữ hoa) tuỳ cài đặt
try:
    import st7735 as _st7735_mod
    _ST7735_CLASS = _st7735_mod.ST7735
except ImportError:
    import ST7735 as _st7735_mod
    _ST7735_CLASS = _st7735_mod.ST7735

# ═══════════════════════════════════════════════════════════
#  CẤU HÌNH
# ═══════════════════════════════════════════════════════════
PIN_DC  = 24    # GPIO24 → Pin 18
PIN_RST = 25    # GPIO25 → Pin 22
SPI_PORT   = 0  # /dev/spidev0.0  (CE0 = GPIO8 / Pin 24)
ROTATION   = 270

DISPLAY_W = 128
DISPLAY_H = 160

MONITOR_INTERVAL = int(os.environ.get("MONITOR_INTERVAL", 10))
API_JS = Path(__file__).parent / "api.js"

# ═══════════════════════════════════════════════════════════
#  MÀU SẮC (RGB)
# ═══════════════════════════════════════════════════════════
BG         = (0,   0,   0)
CYAN       = (0,   220, 220)
YELLOW     = (255, 220, 0)
GREEN      = (50,  220, 80)
RED        = (255, 60,  60)
WHITE      = (255, 255, 255)
GRAY       = (160, 160, 160)
DARK_GRAY  = (50,  50,  50)
ORANGE     = (255, 160, 0)
MAGENTA    = (255, 60,  200)
BLUE_DARK  = (0,   30,  80)
TEAL       = (0,   180, 160)


# ═══════════════════════════════════════════════════════════
#  FONTS — tải từ hệ thống, fallback về default
# ═══════════════════════════════════════════════════════════
def _font(size: int, bold=False):
    candidates = [
        f"/usr/share/fonts/truetype/dejavu/DejaVuSans{'Bold' if bold else ''}.ttf",
        f"/usr/share/fonts/truetype/liberation/LiberationSans-{'Bold' if bold else 'Regular'}.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    ]
    for path in candidates:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size)
            except Exception:
                continue
    return ImageFont.load_default()

FONT_LOGO   = _font(11, bold=True)
FONT_LABEL  = _font(9,  bold=True)
FONT_VALUE  = _font(9)
FONT_SMALL  = _font(8)
FONT_TINY   = _font(7)


# ═══════════════════════════════════════════════════════════
#  TRẠNG THÁI DÙNG CHUNG
# ═══════════════════════════════════════════════════════════
_state = {
    "flask":   "...",
    "pb":      "...",
    "tunnel":  "Đang tải...",
    "product": "",        # Tên SP đang AI xử lý
    "source":  "",        # "gemini" / "fallback"
    "logs":    [],        # Tối đa 5 dòng
    "req_count": 0,       # Tổng số request
}
_lock = threading.Lock()


def add_log(msg: str):
    ts = datetime.now().strftime("%H:%M")
    with _lock:
        _state["logs"].insert(0, f"{ts} {msg}")
        _state["logs"] = _state["logs"][:5]

def update_state(**kwargs):
    with _lock:
        _state.update(kwargs)


# ═══════════════════════════════════════════════════════════
#  VẼ MÀN HÌNH
# ═══════════════════════════════════════════════════════════
def _dot(draw, x, y, ok: bool, r=4):
    """Chấm tròn xanh/đỏ."""
    color = GREEN if ok else RED
    draw.ellipse([x-r, y-r, x+r, y+r], fill=color)

def _hline(draw, y, color=DARK_GRAY):
    draw.line([(0, y), (DISPLAY_W, y)], fill=color, width=1)

def _trunc(text: str, max_chars: int) -> str:
    return text if len(text) <= max_chars else text[:max_chars-1] + "…"

def render_frame(state: dict) -> Image.Image:
    img  = Image.new("RGB", (DISPLAY_W, DISPLAY_H), BG)
    draw = ImageDraw.Draw(img)

    # ── HEADER ─────────────────────────────── y=0..16
    draw.rectangle([0, 0, DISPLAY_W, 16], fill=BLUE_DARK)
    # Logo "⚡ ElectroShop"
    draw.text((4, 2),  "ElectroShop",  font=FONT_LOGO,  fill=CYAN)
    draw.text((4, 2),  "E",            font=FONT_LOGO,  fill=YELLOW)  # Nổi bật chữ đầu
    # Thời gian nhỏ góc phải
    ts = datetime.now().strftime("%H:%M")
    draw.text((DISPLAY_W - 28, 4), ts, font=FONT_TINY, fill=GRAY)

    # ── TRẠNG THÁI DỊCH VỤ ──────────────── y=20..48
    y = 20
    # Flask
    ok_flask = state["flask"] == "OK"
    _dot(draw, 6, y+5, ok_flask, r=4)
    draw.text((14, y),  "Flask", font=FONT_LABEL, fill=YELLOW)
    draw.text((52, y),  state["flask"], font=FONT_VALUE,
              fill=GREEN if ok_flask else RED)

    y = 34
    # PocketBase
    ok_pb = state["pb"] == "OK"
    _dot(draw, 6, y+5, ok_pb, r=4)
    draw.text((14, y),  "PocketBase", font=FONT_LABEL, fill=YELLOW)
    draw.text((80, y),  state["pb"], font=FONT_VALUE,
              fill=GREEN if ok_pb else RED)

    _hline(draw, 48)

    # ── TUNNEL LINK ─────────────────────── y=50..62
    y = 51
    draw.text((4, y),  "🔗", font=FONT_TINY,  fill=TEAL)
    draw.text((14, y), _trunc(state["tunnel"], 20), font=FONT_SMALL, fill=TEAL)

    _hline(draw, 63)

    # ── AI ĐANG XỬ LÝ ───────────────────── y=65..90
    y = 65
    draw.text((4, y), "> AI đang xử lý:", font=FONT_LABEL, fill=ORANGE)

    y = 77
    product = state["product"]
    if product:
        # Xuống dòng nếu dài (≤19 ký tự/dòng với font size 9)
        if len(product) <= 19:
            draw.text((4, y), product, font=FONT_SMALL, fill=MAGENTA)
        else:
            draw.text((4, y),    product[:19],  font=FONT_SMALL, fill=MAGENTA)
            draw.text((4, y+11), _trunc(product[19:], 19), font=FONT_SMALL, fill=MAGENTA)
    else:
        draw.text((4, y), "(chờ yêu cầu...)", font=FONT_SMALL, fill=DARK_GRAY)

    # Badge nguồn AI
    if state["source"] == "gemini":
        draw.rectangle([DISPLAY_W-44, 65, DISPLAY_W-2, 75], fill=(0, 60, 20))
        draw.text((DISPLAY_W-42, 66), "Gemini✓", font=FONT_TINY, fill=GREEN)
    elif state["source"] == "fallback":
        draw.rectangle([DISPLAY_W-50, 65, DISPLAY_W-2, 75], fill=(60, 40, 0))
        draw.text((DISPLAY_W-48, 66), "Fallback", font=FONT_TINY, fill=ORANGE)

    _hline(draw, 100)

    # ── 5 DÒNG LOG ──────────────────────── y=102..155
    y = 103
    draw.text((4, y), "Logs:", font=FONT_LABEL, fill=YELLOW)
    y = 114
    logs = state["logs"]
    for i, line in enumerate(logs[:5]):
        if y > DISPLAY_H - 9:
            break
        color = WHITE if i == 0 else GRAY
        draw.text((4, y), _trunc(line, 21), font=FONT_TINY, fill=color)
        y += 10

    # ── FOOTER ─────────────────────────── y=153..160
    _hline(draw, 152, DARK_GRAY)
    req = state.get("req_count", 0)
    draw.text((4, 153), f"Req: {req}", font=FONT_TINY, fill=DARK_GRAY)

    return img


# ═══════════════════════════════════════════════════════════
#  POLL TRẠNG THÁI
# ═══════════════════════════════════════════════════════════
def _check(url: str, timeout=3) -> str:
    try:
        r = requests.get(url, timeout=timeout)
        return "OK" if r.status_code == 200 else f"ERR{r.status_code}"
    except Exception:
        return "OFFLINE"

def _read_tunnel() -> str:
    try:
        text = API_JS.read_text()
        m = re.search(r"CLOUD_LINK\s*=\s*['\"]([^'\"]+)['\"]", text)
        if m:
            return m.group(1).replace("https://", "").split(".")[0][:20]
    except Exception:
        pass
    return "N/A"

def _poll_once():
    flask_st = _check("http://127.0.0.1:5001/health")
    pb_st    = _check("http://127.0.0.1:8090/api/health")
    tunnel   = _read_tunnel()
    with _lock:
        changed = (
            _state["flask"]  != flask_st or
            _state["pb"]     != pb_st    or
            _state["tunnel"] != tunnel
        )
        _state["flask"]  = flask_st
        _state["pb"]     = pb_st
        _state["tunnel"] = tunnel
    if changed:
        add_log(f"F:{flask_st} PB:{pb_st}")


# ═══════════════════════════════════════════════════════════
#  API CÔNG KHAI — gọi từ auto_desc.py
# ═══════════════════════════════════════════════════════════
def notify_ai_start(product_name: str):
    """Hiển thị tên SP đang xử lý lên màn hình."""
    with _lock:
        _state["product"] = product_name
        _state["source"]  = ""
        _state["req_count"] += 1
    add_log(f"AI→ {product_name[:15]}")

def notify_ai_done(product_name: str, source: str):
    """Cập nhật kết quả sau khi AI xử lý xong."""
    label = "Gemini✓" if source == "gemini" else "Fbk"
    add_log(f"{label} {product_name[:13]}")
    with _lock:
        _state["product"] = ""
        _state["source"]  = source



# ═══════════════════════════════════════════════════════════
#  DISPLAY INIT & REINIT
# ═══════════════════════════════════════════════════════════

# Sau bao nhiêu giây không render thành công → tự reinit
REINIT_AFTER_SECS = 30
# Số lần lỗi SPI liên tiếp trước khi reinit
MAX_SPI_ERRORS = 3

def _create_display():
    """Tạo đối tượng ST7735 với tham số INITR_BLACKTAB chuẩn.
    
    BLACKTAB = invert=False + bgr=True + INVOFF hardware (0x20)
    KHÔNG dùng invert=True vì thư viện sẽ gửi INVON (0x21) làm đảo màu.
    """
    return _ST7735_CLASS(
        port=SPI_PORT, cs=0, dc=PIN_DC, rst=PIN_RST,
        rotation=ROTATION, width=DISPLAY_W, height=DISPLAY_H,
        offset_left=0, offset_top=0,
        invert=False,   # INITR_BLACKTAB: không đảo phần mềm
        bgr=True,       # byte order BGR (đúng cho BLACKTAB)
        spi_speed_hz=4_000_000,
    )

def _init_sequence(disp):
    """Chạy chuỗi khởi tạo phần cứng sau khi begin()."""
    disp.begin()
    disp.command(0x20)   # INVOFF — ép phần cứng ở chế độ BLACKTAB
    time.sleep(0.05)
    # Xoá màn hình về đen để tránh rác trên màn hình
    blank = Image.new("RGB", (disp.width, disp.height), (0, 0, 0))
    disp.display(blank)

def reinit_display(disp):
    """Khởi động lại màn hình khi phát hiện màn hình trắng/mất kết nối.
    
    Nguyên nhân thường gặp:
    - Chạm tay gây nhiễu SPI
    - Dao động nguồn reset controller
    - Lỗi SPI bus tạm thời
    """
    print(f"[Monitor] [{datetime.now().strftime('%H:%M:%S')}] REINIT màn hình ST7735...")
    add_log("REINIT display!")
    try:
        _init_sequence(disp)
        print("[Monitor] REINIT OK!")
        add_log("REINIT OK")
        return True
    except Exception as e:
        print(f"[Monitor] REINIT THAT BAI: {e}")
        add_log(f"REINIT ERR:{str(e)[:10]}")
        return False


# ═══════════════════════════════════════════════════════════
#  MAIN — INITR_BLACKTAB + watchdog reinit_display()
# ═══════════════════════════════════════════════════════════
def main():
    print("[Monitor] Khoi dong ST7735 (INITR_BLACKTAB)...")
    print(f"[Monitor] DC=GPIO{PIN_DC} RST=GPIO{PIN_RST} SPI{SPI_PORT}/CE0 rot={ROTATION}")

    try:
        disp = _create_display()
        _init_sequence(disp)
        print("[Monitor] ST7735 init OK! (BLACKTAB: invert=False, INVOFF=0x20)")
    except Exception as e:
        print(f"[Monitor] LOI KHOI DONG: {e}")
        sys.exit(1)

    add_log("Monitor started")
    _poll_once()

    last_poll = 0.0
    last_render_ok = time.time()
    spi_error_count = 0

    try:
        while True:
            now = time.time()

            # --- Poll trạng thái dịch vụ định kỳ ---
            if now - last_poll >= MONITOR_INTERVAL:
                _poll_once()
                last_poll = now

            # --- Watchdog: nếu quá lâu không render được → reinit ---
            if now - last_render_ok > REINIT_AFTER_SECS:
                print(f"[Monitor] Watchdog: {REINIT_AFTER_SECS}s không render — reinit...")
                reinit_display(disp)
                last_render_ok = time.time()
                spi_error_count = 0
                time.sleep(0.5)
                continue

            # --- Lấy snapshot trạng thái ---
            with _lock:
                snap = dict(_state)
                snap["logs"] = list(_state["logs"])

            frame = render_frame(snap)

            # --- Gọi display() bọc try/except để bắt lỗi SPI ---
            try:
                disp.display(frame)
                last_render_ok = time.time()
                spi_error_count = 0          # reset counter khi thành công
            except Exception as e:
                spi_error_count += 1
                print(f"[Monitor] Loi display() #{spi_error_count}: {e}")
                if spi_error_count >= MAX_SPI_ERRORS:
                    print("[Monitor] Qua nhieu loi SPI — reinit ngay!")
                    reinit_display(disp)
                    last_render_ok = time.time()
                    spi_error_count = 0
                time.sleep(0.5)
                continue

            time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n[Monitor] Dung.")


if __name__ == "__main__":
    main()
