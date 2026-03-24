#!/usr/bin/env python3
"""
auto_desc.py — Flask server sinh mô tả sản phẩm bằng Gemini AI
Chạy: python3 auto_desc.py
Cổng: 5001 (hoặc set AUTO_DESC_PORT=xxxx)

Yêu cầu: export GEMINI_API_KEY="AIza..."
"""

import os
import sys
import json
import threading
from datetime import datetime
from pathlib import Path
from flask import Flask, request, jsonify
from flask_cors import CORS
from google import genai

ORDER_FILE = Path("/tmp/new_order.json")
STATE_FILE = Path("/tmp/electroshop_state.json")

# ── Cấu hình ──────────────────────────────────────────────
PORT         = int(os.environ.get("AUTO_DESC_PORT", 5001))
GEMINI_MODEL = "models/gemini-2.5-flash"

app = Flask(__name__)
CORS(app)  # Cho phép admin.html gọi từ mọi origin


# ── Helpers ───────────────────────────────────────────────

def _get_client():
    key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not key:
        raise EnvironmentError("GEMINI_API_KEY chưa được đặt")
    return genai.Client(api_key=key)


def _build_prompt(ten: str, gia: float, loai: str) -> str:
    gia_str  = f"{int(gia):,}".replace(",", ".") + "₫"
    loai_str = f" (danh mục: {loai})" if loai else ""
    return (
        f"Bạn là chuyên gia viết nội dung thương mại điện tử tiếng Việt.\n"
        f"Viết mô tả sản phẩm ngắn gọn, chuyên nghiệp, hấp dẫn bằng tiếng Việt "
        f"(2–4 câu, 60–120 từ) cho:\n\n"
        f"Tên: {ten}\nGiá: {gia_str}{loai_str}\n\n"
        f"Yêu cầu: nêu bật ưu điểm chính, giọng văn cuốn hút, "
        f"KHÔNG dùng markdown hay gạch đầu dòng, chỉ trả về đoạn văn thuần."
    )


def _fallback(ten: str, gia: float) -> str:
    gia_str = f"{int(gia):,}".replace(",", ".") + "₫"
    return (
        f"{ten} là sản phẩm chất lượng với giá {gia_str} hợp lý. "
        f"Thiết kế tinh tế, đáp ứng nhu cầu sử dụng hàng ngày. "
        f"Hàng chính hãng, bảo hành đầy đủ."
    )


# ── Routes ────────────────────────────────────────────────

@app.route("/health", methods=["GET"])
def health():
    has_key = bool(os.environ.get("GEMINI_API_KEY", "").strip())
    return jsonify({
        "status":  "ok",
        "model":   GEMINI_MODEL,
        "gemini":  "✅ key đã có" if has_key else "⚠️ chưa có API key",
    })


@app.route("/generate", methods=["POST"])
def generate():
    body = request.get_json(force=True, silent=True) or {}
    ten  = str(body.get("ten",  "")).strip()
    gia  = float(body.get("gia",  0) or 0)
    loai = str(body.get("loai", "")).strip()

    if not ten:
        return jsonify({"error": "Thiếu trường 'ten'"}), 400

    # 📺 Thông báo lên màn hình ST7735 (non-blocking)
    _notify_monitor("start", ten)

    try:
        client   = _get_client()
        prompt   = _build_prompt(ten, gia, loai)
        response = client.models.generate_content(model=GEMINI_MODEL, contents=prompt)
        mo_ta    = response.text.strip()
        source   = "gemini"
    except EnvironmentError as e:
        print(f"[auto_desc] ⚠️  {e}", file=sys.stderr)
        mo_ta  = _fallback(ten, gia)
        source = "fallback"
    except Exception as e:
        print(f"[auto_desc] ❌ Gemini lỗi: {e}", file=sys.stderr)
        mo_ta  = _fallback(ten, gia)
        source = "fallback"

    # 📺 Thông báo hoàn thành lên màn hình
    _notify_monitor("done", ten, source)

    return jsonify({"mo_ta": mo_ta, "source": source, "ten": ten, "gia": gia})


@app.route("/monitor-notify", methods=["POST"])
def monitor_notify():
    """Cập nhật state file cho pi_debug.py đọc."""
    body = request.get_json(force=True, silent=True) or {}
    event   = body.get("event", "info")
    product = str(body.get("product", "")).strip()
    source  = str(body.get("source", "")).strip()
    _write_state(event, product, source)
    return jsonify({"ok": True})


@app.route("/notify-order", methods=["POST"])
def notify_order():
    """Nhận đơn hàng mới → ghi file để pi_debug.py hiện CÓ ĐƠN MỚI."""
    body = request.get_json(force=True, silent=True) or {}
    order = {
        "name":    str(body.get("name",    "")).strip(),
        "phone":   str(body.get("phone",   "")).strip(),
        "address": str(body.get("address", "")).strip(),
        "total":   body.get("total", 0),
        "items":   body.get("items", []),
        "time":    datetime.now().strftime("%d/%m %H:%M"),
    }
    try:
        ORDER_FILE.write_text(json.dumps(order, ensure_ascii=False), encoding="utf-8")
        print(f"[auto_desc] 🔔 ĐƠN MỚI từ {order['name']} — {order['total']:,}₫", file=sys.stderr)
    except Exception as e:
        print(f"[auto_desc] Lỗi ghi order file: {e}", file=sys.stderr)
    return jsonify({"ok": True, "message": "Đặt hàng thành công!"})


def _write_state(event: str, product: str, source: str = ""):
    try:
        STATE_FILE.write_text(
            json.dumps({"event": event, "product": product, "source": source,
                        "ts": datetime.now().isoformat()}, ensure_ascii=False),
            encoding="utf-8"
        )
    except Exception as e:
        print(f"[auto_desc] Lỗi ghi state: {e}", file=sys.stderr)


def _notify_monitor(event: str, product: str, source: str = ""):
    """Ghi state file non-blocking để cập nhật màn hình ST7735."""
    threading.Thread(target=_write_state, args=(event, product, source), daemon=True).start()


# ── Entry point ───────────────────────────────────────────

if __name__ == "__main__":
    has_key = bool(os.environ.get("GEMINI_API_KEY", "").strip())
    print("=" * 50)
    print("  🤖 ElectroShop — Auto Description Server")
    print("=" * 50)
    print(f"  Model  : {GEMINI_MODEL}")
    print(f"  API Key: {'✅ Đã có' if has_key else '⚠️  Chưa có — dùng fallback'}")
    print(f"  URL    : http://0.0.0.0:{PORT}")
    print("=" * 50)
    app.run(host="0.0.0.0", port=PORT, debug=False)
