#!/usr/bin/env python3
"""
cyd_serial.py — Gửi dữ liệu ElectroShop lên CYD ESP32-2432S035 qua USB Serial
═══════════════════════════════════════════════════════════════════════════════
Pi 5 → /dev/ttyUSB0 (CH340C) → ESP32 → màn hình ILI9488 3.5"

Gửi JSON mỗi 3 giây:
  - Giờ / ngày thực
  - Trạng thái Tunnel / Flask / PocketBase
  - 3 đơn hàng gần nhất (từ PocketBase collection 'donhang')
  - 3 tin tức gần nhất (từ PocketBase collection 'tintuc')
  - Thống kê tổng: sản phẩm, đơn hàng, tin tức
  - Tài nguyên Pi 5: CPU temp, RAM%, Disk%, CPU%, IP
"""

import os, re, sys, time, json, threading, subprocess, socket
from datetime import datetime, timezone
import requests
import serial
import serial.tools.list_ports

# ── Cấu hình ──────────────────────────────────────────────
SERIAL_PORT  = os.environ.get("CYD_PORT", "/dev/ttyUSB0")
SERIAL_BAUD  = 115200
SEND_INTERVAL = 3          # giây giữa mỗi lần gửi
POLL_INTERVAL = 8          # giây giữa mỗi lần hỏi PocketBase
PB_URL        = "http://127.0.0.1:8090"
FLASK_URL     = "http://127.0.0.1:5001/health"
API_JS_PATH   = os.path.join(os.path.dirname(__file__), "api.js")
CF_LOG        = "/tmp/cf_electroshop.log"

# ── Trạng thái dùng chung ─────────────────────────────────
_data = {
    "tunnel":     "...",
    "tunnel_url": "",
    "flask":      "...",
    "pb":         "...",
    "orders":     [],
    "news":       [],
    "stats":      {"products": 0, "orders_total": 0, "news_total": 0},
    "sys":        {"cpu_temp": 0, "ram_pct": 0, "disk_pct": 0, "cpu_pct": 0, "ip": ""},
}
_lock = threading.Lock()


# ═══════════════════════════════════════════════════════════
#  SYS INFO (Pi 5)
# ═══════════════════════════════════════════════════════════
def _cpu_temp() -> float:
    try:
        raw = open("/sys/class/thermal/thermal_zone0/temp").read().strip()
        return round(int(raw) / 1000.0, 1)
    except Exception:
        try:
            out = subprocess.getoutput("vcgencmd measure_temp")
            m = re.search(r"[\d.]+", out)
            return float(m.group()) if m else 0.0
        except Exception:
            return 0.0

def _ram_pct() -> int:
    try:
        out = subprocess.getoutput("free -m")
        lines = out.splitlines()
        for line in lines:
            if line.startswith("Mem:"):
                parts = line.split()
                total, used = int(parts[1]), int(parts[2])
                return int(used * 100 / total) if total > 0 else 0
    except Exception:
        pass
    return 0

def _disk_pct() -> int:
    try:
        out = subprocess.getoutput("df / --output=pcent | tail -1")
        return int(out.strip().replace("%", ""))
    except Exception:
        return 0

def _cpu_pct() -> int:
    try:
        out = subprocess.getoutput(
            "top -bn1 | grep 'Cpu(s)' | awk '{print $2}'"
        )
        return int(float(out.strip()))
    except Exception:
        return 0

def _local_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return ""

def _get_sys_info() -> dict:
    return {
        "cpu_temp": _cpu_temp(),
        "ram_pct":  _ram_pct(),
        "disk_pct": _disk_pct(),
        "cpu_pct":  _cpu_pct(),
        "ip":       _local_ip(),
    }


# ═══════════════════════════════════════════════════════════
#  TUNNEL URL
# ═══════════════════════════════════════════════════════════
def _read_tunnel() -> tuple:
    """Trả về (status 'ON'/'OFF', url)."""
    url = ""
    try:
        if os.path.exists(CF_LOG):
            m = re.search(
                r"https://[a-zA-Z0-9\-]+\.trycloudflare\.com", 
                open(CF_LOG).read()
            )
            if m:
                url = m.group(0)
    except Exception:
        pass

    if not url:
        # Thử đọc từ api.js
        try:
            text = open(API_JS_PATH).read()
            m = re.search(r"CLOUD_LINK\s*=\s*['\"]([^'\"]+)['\"]", text)
            if m:
                url = m.group(1)
        except Exception:
            pass

    if url:
        # Kiểm tra tunnel còn sống không
        try:
            r = requests.get(url + "/api/health", timeout=4)
            if r.status_code < 500:
                return "ON", url
        except Exception:
            return "OFF", url
    return "OFF", url


# ═══════════════════════════════════════════════════════════
#  POCKETBASE QUERIES
# ═══════════════════════════════════════════════════════════
def _age_str(created_str: str) -> str:
    """Chuyển ISO timestamp → chuỗi 'X phut/gio/ngay'."""
    try:
        # PocketBase format: 2025-01-15 10:30:00.000Z
        created_str = created_str.replace(" ", "T")
        if not created_str.endswith("Z"):
            created_str += "Z"
        dt = datetime.fromisoformat(created_str.replace("Z", "+00:00"))
        now = datetime.now(timezone.utc)
        diff = int((now - dt).total_seconds())
        if diff < 60:    return f"{diff}s"
        if diff < 3600:  return f"{diff//60}p"
        if diff < 86400: return f"{diff//3600}h"
        return f"{diff//86400}ng"
    except Exception:
        return "?"

def _fetch_orders() -> tuple:
    """Trả về (list 3 đơn gần nhất, tổng số đơn)."""
    try:
        r = requests.get(
            f"{PB_URL}/api/collections/donhang/records",
            params={"sort": "-created", "perPage": 3},
            timeout=4
        )
        if r.status_code == 200:
            data = r.json()
            orders = []
            for item in data.get("items", []):
                name  = str(item.get("ten_sp", item.get("sanpham", "?")))[:38]
                price = str(item.get("gia", item.get("tong_tien", "0")))
                buyer = str(item.get("ten_kh", item.get("khachhang", "?")))[:18]
                age   = _age_str(item.get("created", ""))
                orders.append({"name": name, "price": price, "by": buyer, "age": age})
            total = data.get("totalItems", 0)
            return orders, total
    except Exception:
        pass
    return [], 0

def _fetch_news() -> tuple:
    """Trả về (list 3 tin gần nhất, tổng số tin)."""
    try:
        r = requests.get(
            f"{PB_URL}/api/collections/tintuc/records",
            params={"sort": "-created", "perPage": 3},
            timeout=4
        )
        if r.status_code == 200:
            data = r.json()
            news = []
            for item in data.get("items", []):
                title = str(item.get("tieu_de", item.get("title", "?")))[:56]
                cat   = str(item.get("danh_muc", item.get("cat", "Tin")))[:12]
                age   = _age_str(item.get("created", ""))
                news.append({"title": title, "cat": cat, "age": age})
            total = data.get("totalItems", 0)
            return news, total
    except Exception:
        pass
    return [], 0

def _fetch_products_count() -> int:
    try:
        r = requests.get(
            f"{PB_URL}/api/collections/sanpham/records",
            params={"perPage": 1},
            timeout=4
        )
        if r.status_code == 200:
            return r.json().get("totalItems", 0)
    except Exception:
        pass
    return 0

def _check_service(url: str) -> str:
    try:
        r = requests.get(url, timeout=3)
        return "ON" if r.status_code == 200 else "OFF"
    except Exception:
        return "OFF"


# ═══════════════════════════════════════════════════════════
#  POLL THREAD — cập nhật _data định kỳ
# ═══════════════════════════════════════════════════════════
def _poll_loop():
    while True:
        try:
            flask_st = _check_service(FLASK_URL)
            pb_st    = _check_service(f"{PB_URL}/api/health")
            tun_st, tun_url = _read_tunnel()

            orders, orders_total = _fetch_orders()
            news, news_total     = _fetch_news()
            products_total       = _fetch_products_count()
            sys_info             = _get_sys_info()

            with _lock:
                _data["flask"]     = flask_st
                _data["pb"]        = pb_st
                _data["tunnel"]    = tun_st
                _data["tunnel_url"]= tun_url
                _data["orders"]    = orders
                _data["news"]      = news
                _data["stats"]     = {
                    "products":     products_total,
                    "orders_total": orders_total,
                    "news_total":   news_total,
                }
                _data["sys"] = sys_info

            print(f"[CYD] Poll: flask={flask_st} pb={pb_st} tunnel={tun_st} "
                  f"orders={len(orders)} news={len(news)} "
                  f"cpu={sys_info['cpu_temp']}C ram={sys_info['ram_pct']}%")
        except Exception as e:
            print(f"[CYD] Poll error: {e}")

        time.sleep(POLL_INTERVAL)


# ═══════════════════════════════════════════════════════════
#  SERIAL SENDER
# ═══════════════════════════════════════════════════════════
def _build_packet() -> bytes:
    now = datetime.now()
    with _lock:
        snap = {
            "t":          now.strftime("%H:%M"),
            "d":          now.strftime("%d/%m"),
            "tunnel":     _data["tunnel"],
            "tunnel_url": _data["tunnel_url"],
            "flask":      _data["flask"],
            "pb":         _data["pb"],
            "orders":     _data["orders"],
            "news":       _data["news"],
            "stats":      _data["stats"],
            "sys":        _data["sys"],
        }
    line = json.dumps(snap, ensure_ascii=False, separators=(",", ":"))
    return (line + "\n").encode("utf-8")

def _find_serial_port() -> str:
    """Tự động tìm cổng CH340C nếu SERIAL_PORT không kết nối được."""
    if os.path.exists(SERIAL_PORT):
        return SERIAL_PORT
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if "ch340" in desc or "usb serial" in desc or "cp210" in desc:
            print(f"[CYD] Tìm thấy cổng serial: {port.device} ({port.description})")
            return port.device
    return SERIAL_PORT

def _serial_loop():
    port = _find_serial_port()
    conn = None
    print(f"[CYD] Kết nối Serial → {port} @ {SERIAL_BAUD}")

    while True:
        # Kết nối / kết nối lại
        if conn is None:
            try:
                conn = serial.Serial(port, SERIAL_BAUD, timeout=1)
                print(f"[CYD] ✅ Serial {port} đã kết nối")
                time.sleep(2)   # Chờ ESP32 boot
            except Exception as e:
                print(f"[CYD] ⚠️  Không thể mở serial: {e}. Thử lại sau 5s...")
                conn = None
                time.sleep(5)
                continue

        # Gửi packet
        try:
            pkt = _build_packet()
            conn.write(pkt)
            conn.flush()
        except Exception as e:
            print(f"[CYD] Lỗi ghi serial: {e}. Đóng kết nối...")
            try:
                conn.close()
            except Exception:
                pass
            conn = None

        time.sleep(SEND_INTERVAL)


# ═══════════════════════════════════════════════════════════
#  ENTRY POINT
# ═══════════════════════════════════════════════════════════
if __name__ == "__main__":
    print("=" * 55)
    print("  CYD Serial Sender — ElectroShop Pi 5")
    print(f"  Port: {SERIAL_PORT}  |  Baud: {SERIAL_BAUD}")
    print(f"  Send every {SEND_INTERVAL}s  |  Poll PB every {POLL_INTERVAL}s")
    print("=" * 55)

    # Chạy poll trong background thread
    t = threading.Thread(target=_poll_loop, daemon=True)
    t.start()

    # Gửi serial ở main thread (loop mãi)
    _serial_loop()
