#!/usr/bin/env python3
"""
CYD YouTube Player - Flask Server (SD Card / High Quality Edition)
Chạy trên Raspberry Pi 5, port 9000
Transcode sang AVI (MJPEG 320x240 @15fps + PCM 8kHz 8-bit mono)
rồi stream về ESP32 CYD lưu vào SD card.
"""

from flask import Flask, request, jsonify, Response
from deep_translator import GoogleTranslator
import yt_dlp
import subprocess
import threading
import os
import glob
import base64
import requests
import logging
import time
import io

from PIL import Image

app = Flask(__name__)
log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(levelname)s %(message)s')

# ── Trạng thái chuẩn bị video ──────────────────────────────────────────────
# {video_id: {"status": "preparing"|"ready"|"error", "progress": 0-100}}
preparing: dict = {}

# ── Trạng thái điều khiển playback (từ /control) ───────────────────────────
control_state: dict = {"action": None}


# ═══════════════════════════════════════════════════════════════════════════
# Cleanup file AVI cũ (> 30 phút) khi khởi động
# ═══════════════════════════════════════════════════════════════════════════
def cleanup_old_files() -> None:
    cutoff = time.time() - 30 * 60  # 30 phút
    for path in glob.glob("/tmp/yt_*.avi"):
        try:
            if os.path.getmtime(path) < cutoff:
                os.remove(path)
                log.info("Da xoa file cu: %s", path)
        except OSError:
            pass


cleanup_old_files()


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /health
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/health")
def health():
    return jsonify({"status": "ok", "version": "1.0"})


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /search?q=<query>
# Tìm 5 video YouTube, trả JSON kèm thumbnail base64
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/search")
def search():
    q = request.args.get("q", "").strip()
    if not q:
        return jsonify([])

    try:
        ydl_opts = {
            "quiet": True,
            "extract_flat": True,
            "noplaylist": True,
        }
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(f"ytsearch5:{q}", download=False)

        entries = info.get("entries") or []
        results = []

        for entry in entries[:5]:
            vid_id = entry.get("id", "")
            title = (entry.get("title") or "")[:40].strip()
            duration = int(entry.get("duration") or 0)

            # ── Tải và resize thumbnail về 120×68 ──────────────────────
            thumb_b64 = ""
            try:
                thumb_url = entry.get("thumbnail") or ""
                if not thumb_url:
                    thumbs = entry.get("thumbnails") or []
                    if thumbs:
                        thumb_url = thumbs[-1].get("url", "")
                if thumb_url:
                    resp = requests.get(thumb_url, timeout=3)
                    img = Image.open(io.BytesIO(resp.content)).convert("RGB")
                    img = img.resize((120, 68), Image.LANCZOS)
                    buf = io.BytesIO()
                    img.save(buf, format="JPEG", quality=70)
                    thumb_b64 = base64.b64encode(buf.getvalue()).decode()
            except Exception as ex:
                log.warning("Loi thumbnail %s: %s", vid_id, ex)
                thumb_b64 = ""

            results.append({
                "id": vid_id,
                "title": title,
                "duration": duration,
                "thumb": thumb_b64,
            })

        return jsonify(results)

    except Exception as ex:
        log.error("Loi search: %s", ex)
        return jsonify([])


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /prepare/<video_id>
# Bắt đầu transcode AVI trong background thread, trả ngay
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/prepare/<video_id>")
def prepare(video_id: str):
    out_path = f"/tmp/yt_{video_id}.avi"

    # File đã sẵn sàng
    if os.path.exists(out_path) and os.path.getsize(out_path) > 0:
        preparing[video_id] = {"status": "ready", "progress": 100}
        return jsonify({"status": "ready", "message": "File da san sang"})

    # Đang chuẩn bị → không chạy lại
    if preparing.get(video_id, {}).get("status") == "preparing":
        return jsonify({"status": "preparing", "message": "Dang xu ly video..."})

    preparing[video_id] = {"status": "preparing", "progress": 0}

    def transcode_worker():
        try:
            log.info("Bat dau transcode: %s", video_id)

            # Bước 1: Lấy direct URL qua yt-dlp
            ydl_opts = {
                "quiet": True,
                "format": "bestvideo[ext=mp4][height<=480]+bestaudio/best[ext=mp4]/best",
            }
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(
                    f"https://www.youtube.com/watch?v={video_id}",
                    download=False,
                )

            # Lấy URL phát (format đơn hoặc format video trong format kép)
            src_url = info.get("url") or ""
            if not src_url:
                fmts = info.get("requested_formats") or []
                if fmts:
                    src_url = fmts[0].get("url", "")

            preparing[video_id]["progress"] = 10
            log.info("Got URL, starting ffmpeg for %s", video_id)

            # Bước 2: ffmpeg transcode → MJPEG 320×240 @15fps + PCM 8kHz mono 8-bit
            cmd = [
                "ffmpeg", "-i", src_url,
                "-vf", (
                    "scale=320:240:force_original_aspect_ratio=decrease,"
                    "pad=320:240:(ow-iw)/2:(oh-ih)/2"
                ),
                "-r", "15",
                "-vcodec", "mjpeg",
                "-q:v", "3",
                "-acodec", "pcm_u8",
                "-ar", "8000",
                "-ac", "1",
                "-f", "avi",
                out_path,
                "-y",
                "-loglevel", "error",
            ]
            proc = subprocess.run(cmd, timeout=600)

            if proc.returncode == 0 and os.path.exists(out_path):
                size_kb = os.path.getsize(out_path) // 1024
                preparing[video_id] = {"status": "ready", "progress": 100}
                log.info("Transcode xong: %s (%d KB)", out_path, size_kb)
            else:
                preparing[video_id] = {"status": "error", "progress": 0}
                log.error("ffmpeg that bai (code %d)", proc.returncode)

        except Exception as ex:
            log.error("Loi transcode %s: %s", video_id, ex)
            preparing[video_id] = {"status": "error", "progress": 0}

    t = threading.Thread(target=transcode_worker, daemon=True)
    t.start()

    return jsonify({"status": "preparing", "message": "Dang xu ly video..."})


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /status/<video_id>
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/status/<video_id>")
def status(video_id: str):
    state = preparing.get(video_id, {"status": "not_found", "progress": 0})
    resp: dict = {"status": state["status"]}
    if state["status"] == "ready":
        path = f"/tmp/yt_{video_id}.avi"
        resp["size"] = os.path.getsize(path) if os.path.exists(path) else 0
    return jsonify(resp)


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /download/<video_id>
# Stream file AVI về client theo từng chunk 8KB
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/download/<video_id>")
def download(video_id: str):
    path = f"/tmp/yt_{video_id}.avi"
    if not os.path.exists(path):
        return jsonify({"error": "File chua san sang"}), 404

    file_size = os.path.getsize(path)

    def generate():
        with open(path, "rb") as f:
            while True:
                chunk = f.read(8192)
                if not chunk:
                    break
                yield chunk

    return Response(
        generate(),
        mimetype="application/octet-stream",
        headers={"Content-Length": str(file_size)},
    )


# ═══════════════════════════════════════════════════════════════════════════
# Route: GET /translate?text=<text>&src=<src>&dest=<dest>
# ═══════════════════════════════════════════════════════════════════════════
@app.route('/translate')
def translate_text():
    text = request.args.get('text', '')
    src  = request.args.get('src',  'vi')
    dest = request.args.get('dest', 'en')
    if not text:
        return jsonify({'error': 'no text'}), 400
    try:
        result = GoogleTranslator(source=src, target=dest).translate(text)
        return jsonify({'translated': result, 'src': src, 'dest': dest})
    except Exception as e:
        return jsonify({'error': str(e)}), 500


# ═══════════════════════════════════════════════════════════════════════════
# Route: POST /control
# Body: {"action": "pause"|"resume"|"seek_fwd"|"seek_bwd"|"vol_up"|"vol_dn"}
# ═══════════════════════════════════════════════════════════════════════════
@app.route("/control", methods=["POST"])
def control():
    data = request.get_json(force=True, silent=True) or {}
    action = data.get("action", "")
    control_state["action"] = action
    log.info("Control action: %s", action)
    return jsonify({"ok": True})


# ═══════════════════════════════════════════════════════════════════════════
# Entrypoint
# ═══════════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    log.info("CYD YouTube Server khoi dong tren port 9000")
    app.run(host="0.0.0.0", port=9000, threaded=True)
