#!/usr/bin/env python3
"""pi_debug.py — ElectroShop debug screen trên ST7735 (Pi 5)
   Dùng Pimoroni st7735 library, INITR_BLACKTAB = invert=True, bgr=True
   CS nối GND → dùng BG_SPI_CS_BACK (CE0), kernel tự xử lý.
"""
import os, re, sys, time, threading, requests, json
from datetime import datetime
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import st7735

# ── Cấu hình màn hình ───────────────────────────────────────
DISP = st7735.ST7735(
    port=0,
    cs=st7735.BG_SPI_CS_BACK,  # CE0 — CS nối GND luôn active
    dc=24,                       # GPIO24 / Pin 18
    rst=25,                      # GPIO25 / Pin 22
    width=128, height=160,
    rotation=180,                # Portrait dọc, lật 180°
    offset_left=0, offset_top=0,
    invert=False,
    bgr=False,
    spi_speed_hz=4_000_000,     # 4MHz ổn định hơn 16MHz
)

API_JS     = Path(__file__).parent / "api.js"
TUNNEL_LOG = Path("/home/tho/tunnel.log")
ORDER_FILE = Path("/tmp/new_order.json")
STATE_FILE = Path("/tmp/electroshop_state.json")
INTERVAL   = int(os.environ.get("DEBUG_INTERVAL", 8))

# ── Fonts ────────────────────────────────────────────────────
def _f(size, bold=False):
    for p in [f"/usr/share/fonts/truetype/dejavu/DejaVuSans{'-Bold' if bold else ''}.ttf",
              "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"]:
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

FB = _f(12, bold=True); FM = _f(10); FS = _f(9); FT = _f(8)

# ── State ────────────────────────────────────────────────────
_logs   = []
_lock   = threading.Lock()
_order  = {"active": False, "name": "", "total": 0, "time": "", "blink": 0}

def log(msg):
    ts = datetime.now().strftime("%H:%M")
    with _lock:
        _logs.insert(0, f"{ts} {msg}")
        del _logs[6:]

def read_order():
    """Đọc đơn hàng mới từ file, trả về dict nếu có."""
    try:
        if ORDER_FILE.exists():
            data = json.loads(ORDER_FILE.read_text(encoding="utf-8"))
            return data
    except Exception:
        pass
    return None

def read_ai_state():
    """Đọc trạng thái AI đang xử lý."""
    try:
        if STATE_FILE.exists():
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except Exception:
        pass
    return None

# ── Data helpers ─────────────────────────────────────────────
def get_tunnel():
    for src in [API_JS, TUNNEL_LOG]:
        try:
            m = re.search(r'([\w\-]+\.trycloudflare\.com)', src.read_text())
            if m: return m.group(1)
        except: pass
    return "no-tunnel"

def check(url):
    try: return "OK" if requests.get(url, timeout=2).ok else "ERR"
    except: return "OFF"

# ── Render ───────────────────────────────────────────────────
def draw_frame(tunnel, flask, pb, order=None, ai_state=None):
    W, H = 128, 160
    img  = Image.new("RGB", (W, H), (0, 0, 0))
    d    = ImageDraw.Draw(img)

    # Header
    d.rectangle([0,0,W,16], fill=(0,40,90))
    d.text((3, 2), "E", font=FB, fill=(255,220,0))
    d.text((13,2), "lectroShop", font=FB, fill=(0,220,220))
    d.text((W-28,4), datetime.now().strftime("%H:%M"), font=FT, fill=(100,100,100))

    # Nếu có đơn mới — hiện banner nhấp nháy
    if order:
        blink = int(time.time() * 2) % 2 == 0
        bg_col = (180,20,20) if blink else (220,50,0)
        d.rectangle([0,18,W,54], fill=bg_col)
        d.text((4, 19), "🔔 CÓ ĐƠN MỚI!", font=FB, fill=(255,255,255))
        name  = (order.get("name","") or "")[:14]
        total = order.get("total", 0)
        t_str = f"{int(total):,}".replace(",",".")+"đ" if total else ""
        d.text((4, 32), name, font=FM, fill=(255,240,120))
        d.text((4, 44), t_str, font=FM, fill=(255,255,255))
        y = 58
    else:
        y = 20

    # Services
    for label, st in [("Flask :5001", flask), ("PocketBase", pb)]:
        ok = st == "OK"
        d.ellipse([3,y+2,9,y+8], fill=(50,220,80) if ok else (255,60,60))
        d.text((13, y), label, font=FM, fill=(255,220,0))
        d.text((W-28, y), st, font=FM, fill=(50,220,80) if ok else (255,60,60))
        y += 13

    # AI state
    if ai_state and ai_state.get("product"):
        ev   = ai_state.get("event","")
        prod = ai_state["product"][:20]
        col  = (0,200,100) if ev == "done" else (255,200,0)
        icon = "✅" if ev == "done" else "⚙️"
        d.text((2, y), f"{icon} {prod}", font=FT, fill=col)
        y += 10

    # Tunnel
    d.line([0,y,W,y], fill=(40,40,40)); y += 3
    short = tunnel[:22] if len(tunnel)<=22 else tunnel[:10]+"..."+tunnel[-9:]
    d.text((2, y), short, font=FS, fill=(0,200,200)); y += 12

    # Logs
    d.line([0,y,W,y], fill=(40,40,40)); y += 3
    d.text((2,y), "Log:", font=FS, fill=(255,180,0)); y += 11
    with _lock: lines = list(_logs[:5])
    for i, ln in enumerate(lines):
        d.text((2, y), ln[:22], font=FT, fill=(255,255,255) if i==0 else (160,160,160))
        y += 10
        if y > H - 8: break

    return img

# ── Main loop ────────────────────────────────────────────────
def main():
    print("[pi_debug] Start — DC=GPIO24 RST=GPIO25")
    log("Khoi dong OK")
    flask = pb = "..."
    tunnel    = get_tunnel()
    last      = 0.0
    order_ts  = None   # timestamp của đơn cuối đã đọc
    ORDER_SHOW_SECS = 30  # hiện banner bao nhiêu giây

    while True:
        now = time.time()
        if now - last >= INTERVAL:
            flask  = check("http://127.0.0.1:5001/health")
            pb     = check("http://127.0.0.1:8090/api/health")
            tunnel = get_tunnel()
            log(f"F:{flask} PB:{pb}")
            last = now

        # Đọc đơn hàng mới
        active_order = None
        order_data = read_order()
        if order_data:
            ts_str = order_data.get("time", "")
            if ts_str != order_ts:
                order_ts = ts_str
                log(f"Don: {order_data.get('name','')[:10]}")
            # Hiện banner trong ORDER_SHOW_SECS giây kể từ khi file được ghi
            try:
                age = now - ORDER_FILE.stat().st_mtime
                if age < ORDER_SHOW_SECS:
                    active_order = order_data
            except Exception:
                pass

        ai_state = read_ai_state()
        DISP.display(draw_frame(tunnel, flask, pb, active_order, ai_state))
        time.sleep(0.5 if active_order else 1)

if __name__ == "__main__":
    main()
