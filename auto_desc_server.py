#!/usr/bin/env python3
"""
auto_desc_server.py — Flask server để PocketBase hook và Admin UI gọi vào
Chạy: python3 auto_desc_server.py
Cổng: 5001
"""

import os
import sys
from flask import Flask, request, jsonify

# Cho phép import auto_desc từ cùng thư mục
sys.path.insert(0, os.path.dirname(__file__))
import auto_desc

app = Flask(__name__)


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "model": auto_desc.GEMINI_MODEL})


@app.route("/generate", methods=["POST"])
def generate():
    data = request.get_json(force=True, silent=True) or {}
    ten = str(data.get("ten", "")).strip()
    gia = float(data.get("gia", 0) or 0)
    loai = str(data.get("loai", "")).strip()

    if not ten:
        return jsonify({"error": "Thiếu tên sản phẩm (ten)"}), 400

    mo_ta = auto_desc.generate(ten, gia, loai)
    return jsonify({"mo_ta": mo_ta, "ten": ten, "gia": gia})


if __name__ == "__main__":
    port = int(os.environ.get("AUTO_DESC_PORT", 5001))
    key = os.environ.get("GEMINI_API_KEY", "")
    if key:
        print(f"[auto_desc_server] ✅ GEMINI_API_KEY đã được đặt")
    else:
        print(f"[auto_desc_server] ⚠️  GEMINI_API_KEY chưa có — sẽ dùng mô tả mặc định")
    print(f"[auto_desc_server] 🚀 Đang chạy tại http://0.0.0.0:{port}")
    app.run(host="0.0.0.0", port=port, debug=False)
