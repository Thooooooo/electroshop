#!/usr/bin/env python3
"""
monitor_st7735.py — Daemon giám sát hệ thống ElectroShop
Gửi trạng thái + tên sản phẩm đang xử lý lên ESP32 qua HTTP hoặc Serial.

Cấu hình qua biến môi trường:
  ESP32_IP    = 192.168.1.x   (ưu tiên HTTP, mặc định: tự dò)
  ESP32_PORT  = /dev/ttyUSB0  (fallback Serial nếu không có IP)
  MONITOR_INTERVAL = 10       (giây giữa mỗi lần poll)
"""

import os, time, threading, requests, json, re
from datetime import datetime

# ── Cấu hình ──────────────────────────────────────────────────────────────────
ESP32_IP          = os.environ.get("ESP32_IP", "")
ESP32_SERIAL_PORT = os.environ.get("ESP32_PORT", "/dev/ttyUSB0")
ESP32_SERIAL_BAUD = 115200
MONITOR_INTERVAL  = int(os.environ.get("MONITOR_INTERVAL", 10))

FLASK_URL    = "http://127.0.0.1:5001/health"
PB_URL       = "http://127.0.0.1:8090/api/health"
API_JS_PATH  = os.path.join(os.path.dirname(__file__), "api.js")

# Trạng thái dùng chung giữa các thread
_state = {
    "flask":   "...",
    "pb":      "...",
    "tunnel":  "...",
    "product": "",       # Tên SP đang được AI xử lý
    "logs":    [],       # Tối đa 5 dòng log
}
_lock = threading.Lock()
_serial_conn = None      # Serial connection (nếu dùng)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _now():
    return datetime.now().strftime("%H:%M:%S")

def add_log(msg: str):
    with _lock:
        _state["logs"].insert(0, f"{_now()} {msg}")
        _state["logs"] = _state["logs"][:5]

def _check_service(url: str, timeout=3) -> str:
    try:
        r = requests.get(url, timeout=timeout)
        return "OK" if r.status_code == 200 else f"ERR {r.status_code}"
    except Exception:
        return "OFFLINE"

def _read_tunnel() -> str:
    try:
        with open(API_JS_PATH) as f:
            m = re.search(r"CLOUD_LINK\s*=\s*['\"]([^'\"]+)['\"]", f.read())
            if m:
                domain = m.group(1).replace("https://", "").split(".")[0]
                return domain[:18]  # Cắt ngắn cho màn hình nhỏ
    except Exception:
        pass
    return "N/A"


# ── Gửi đến ESP32 ─────────────────────────────────────────────────────────────

def _send_http(payload: dict) -> bool:
    if not ESP32_IP:
        return False
    try:
        r = requests.post(
            f"http://{ESP32_IP}/update",
            json=payload,
            timeout=2
        )
        return r.status_code == 200
    except Exception as e:
        print(f"[Monitor] HTTP→ESP32 lỗi: {e}")
        return False

def _send_serial(payload: dict) -> bool:
    global _serial_conn
    try:
        import serial
        if _serial_conn is None or not _serial_conn.is_open:
            _serial_conn = serial.Serial(ESP32_SERIAL_PORT, ESP32_SERIAL_BAUD, timeout=1)
            time.sleep(2)  # Chờ ESP32 reset
        line = json.dumps(payload, ensure_ascii=False) + "\n"
        _serial_conn.write(line.encode("utf-8"))
        return True
    except Exception as e:
        print(f"[Monitor] Serial→ESP32 lỗi: {e}")
        _serial_conn = None
        return False

def send_to_esp32(payload: dict):
    """Gửi payload lên ESP32 — thử HTTP trước, fallback Serial."""
    if _send_http(payload):
        return
    _send_serial(payload)

def push_update():
    """Lấy trạng thái mới và đẩy lên ESP32."""
    with _lock:
        payload = {
            "flask":   _state["flask"],
            "pb":      _state["pb"],
            "tunnel":  _state["tunnel"],
            "product": _state["product"],
            "logs":    _state["logs"],
        }
    send_to_esp32(payload)


# ── Hàm công khai — gọi từ auto_desc.py ──────────────────────────────────────

def notify_ai_start(product_name: str):
    """Gọi khi bắt đầu xử lý AI — bắn tên SP lên màn hình ngay lập tức."""
    with _lock:
        _state["product"] = product_name
    add_log(f"AI: {product_name[:16]}")
    threading.Thread(target=push_update, daemon=True).start()

def notify_ai_done(product_name: str, source: str):
    """Gọi khi AI xử lý xong."""
    label = "Gemini✓" if source == "gemini" else "Fallback"
    add_log(f"{label}: {product_name[:12]}")
    with _lock:
        _state["product"] = ""
    threading.Thread(target=push_update, daemon=True).start()


# ── Vòng lặp chính ────────────────────────────────────────────────────────────

def _poll_loop():
    print(f"[Monitor] 🚀 Bắt đầu giám sát (mỗi {MONITOR_INTERVAL}s)")
    while True:
        flask_st = _check_service(FLASK_URL)
        pb_st    = _check_service(PB_URL)
        tunnel   = _read_tunnel()

        changed = False
        with _lock:
            if _state["flask"] != flask_st:
                _state["flask"] = flask_st
                changed = True
            if _state["pb"] != pb_st:
                _state["pb"] = pb_st
                changed = True
            if _state["tunnel"] != tunnel:
                _state["tunnel"] = tunnel
                changed = True

        if changed:
            add_log(f"F:{flask_st} PB:{pb_st}")
            print(f"[Monitor] Flask={flask_st} | PB={pb_st} | Tunnel={tunnel}")

        push_update()
        time.sleep(MONITOR_INTERVAL)


# ── Tự dò IP của ESP32 trong mạng LAN ─────────────────────────────────────────

def _discover_esp32(subnet="192.168.1", timeout=0.5):
    """Scan mạng LAN tìm ESP32 đang chạy server /status."""
    print("[Monitor] Đang tìm ESP32 trong mạng LAN...")
    for i in range(1, 255):
        ip = f"{subnet}.{i}"
        try:
            r = requests.get(f"http://{ip}/status", timeout=timeout)
            if r.status_code == 200 and "display" in r.text:
                print(f"[Monitor] ✅ Tìm thấy ESP32 tại: {ip}")
                return ip
        except Exception:
            pass
    return ""


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    if not ESP32_IP:
        # Thử tự dò trong subnet mặc định
        import socket
        try:
            local_ip = socket.gethostbyname(socket.gethostname())
            subnet = ".".join(local_ip.split(".")[:3])
        except Exception:
            subnet = "192.168.1"
        found = _discover_esp32(subnet)
        if found:
            ESP32_IP = found
        else:
            print(f"[Monitor] ⚠️  Không tìm thấy ESP32 qua HTTP → thử Serial {ESP32_SERIAL_PORT}")

    add_log("Monitor khoi dong")
    _poll_loop()
