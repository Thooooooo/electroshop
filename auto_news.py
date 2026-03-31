#!/usr/bin/env python3
"""
auto_news.py — Tự động viết bài báo điện tử bằng Gemini AI + ảnh Unsplash
Chạy thủ công : python3 auto_news.py
Chạy tự động  : cron (mỗi 4 tiếng)

Mỗi lần chạy sẽ tạo 1 bài báo mới và lưu vào DB.
"""

import os, sys, json, sqlite3, random, time, requests
from datetime import datetime, timezone
from pathlib import Path
import google.generativeai as genai_lib

# ── Cấu hình ──────────────────────────────────────────────────────────────────
DB_PATH      = Path(__file__).parent / "v2.db"
GEMINI_KEY   = os.environ.get("GEMINI_API_KEY", "AIzaSyCZe1zh64PDeWzE_1j43zlUmhEVzQPhDmM")
GEMINI_MODEL = "models/gemini-2.5-flash"
AUTHOR       = "ElectroBot AI"

# Chủ đề xoay vòng mỗi lần chạy (đủ đa dạng)
TOPICS = [
    ("Raspberry Pi 5", "Vi tính nhúng",
     "electronics,raspberry pi,circuit board"),
    ("Arduino ESP32", "Vi điều khiển",
     "arduino,electronics,microcontroller"),
    ("Luckfox Pico Linux", "Board Linux nhỏ",
     "linux,circuit,technology"),
    ("Cảm biến IoT 2025", "IoT & Cảm biến",
     "sensors,iot,technology"),
    ("In 3D linh kiện điện tử", "Công nghệ mới",
     "3d printing,technology,maker"),
    ("Lập trình Python nhúng", "Lập trình",
     "python,programming,code"),
    ("Camera AI nhỏ gọn", "AI & Camera",
     "camera,artificial intelligence,technology"),
    ("Pin LiPo & sạc nhanh", "Phần cứng",
     "battery,electronics,power"),
    ("PCB thiết kế tại nhà", "DIY",
     "circuit board,diy,electronics"),
    ("RTOS & FreeRTOS", "Hệ điều hành nhúng",
     "embedded,microcontroller,software"),
    ("Màn hình TFT SPI nhỏ", "Hiển thị",
     "display,electronics,screen"),
    ("Motor driver điều khiển robot", "Robot",
     "robot,motor,electronics"),
]

# ── Lấy ảnh từ Unsplash (không cần API key) ───────────────────────────────────
def get_image_url(keywords: str, width=800, height=450) -> str:
    """Dùng source.unsplash.com — free, không cần key, luôn trả ảnh đẹp."""
    slug = keywords.replace(",", ",").replace(" ", "%20")
    # Thêm random seed để tránh ảnh bị lặp
    seed = random.randint(1000, 9999)
    return f"https://source.unsplash.com/{width}x{height}/?{slug}&sig={seed}"

# ── Fallback nếu Gemini lỗi ───────────────────────────────────────────────────
def fallback_article(topic: str, category: str) -> dict:
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    return {
        "title":   f"Xu hướng {topic} năm 2026",
        "summary": f"{topic} đang trở thành công nghệ không thể thiếu trong các dự án điện tử hiện đại.",
        "content": (
            f"Trong năm 2026, {topic} tiếp tục khẳng định vị thế của mình trong lĩnh vực {category}. "
            f"Với giá thành ngày càng giảm và hiệu năng cải thiện đáng kể, đây là thời điểm tốt nhất "
            f"để bắt đầu học và ứng dụng {topic} vào các dự án thực tế.\n\n"
            f"ElectroShop luôn cập nhật các sản phẩm {topic} mới nhất với giá tốt nhất thị trường."
        ),
        "category": category,
    }

# ── Gọi Gemini sinh bài báo ───────────────────────────────────────────────────
def generate_article(topic: str, category: str) -> dict:
    prompt = f"""Bạn là phóng viên công nghệ điện tử chuyên nghiệp. Hãy viết 1 bài báo ngắn bằng tiếng Việt về chủ đề:

Chủ đề: {topic}
Danh mục: {category}
Ngày: {datetime.now().strftime("%d/%m/%Y")}

Yêu cầu:
- Tiêu đề hấp dẫn, cụ thể, có năm 2025/2026
- Tóm tắt 1-2 câu (summary)  
- Nội dung 3-4 đoạn, mỗi đoạn 3-4 câu, thông tin thực tế, chuyên sâu
- Giọng văn chuyên nghiệp nhưng dễ hiểu
- KHÔNG dùng markdown, chỉ xuống dòng thường

Trả về JSON đúng format sau (không thêm gì khác):
{{
  "title": "...",
  "summary": "...",
  "content": "...",
  "category": "{category}"
}}"""

    try:
        genai_lib.configure(api_key=GEMINI_KEY)
        model = genai_lib.GenerativeModel(GEMINI_MODEL.replace("models/", ""))
        resp = model.generate_content(prompt)
        text = resp.text.strip()
        # Bóc JSON ra khỏi markdown code block nếu có
        if "```" in text:
            text = text.split("```")[1]
            if text.startswith("json"):
                text = text[4:]
        return json.loads(text.strip())
    except Exception as e:
        print(f"  ⚠️  Gemini lỗi: {e} — dùng fallback")
        return fallback_article(topic, category)

# ── Lưu vào SQLite ────────────────────────────────────────────────────────────
def save_to_db(article: dict, image_url: str) -> int:
    con = sqlite3.connect(DB_PATH)
    try:
        cur = con.execute(
            "INSERT INTO news (title, summary, content, cover, author, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (
                article["title"],
                article["summary"],
                article["content"],
                image_url,
                AUTHOR,
                datetime.now(timezone.utc).isoformat(),
            )
        )
        con.commit()
        return cur.lastrowid
    finally:
        con.close()

# ── Chọn chủ đề chưa viết gần đây ────────────────────────────────────────────
def pick_topic() -> tuple:
    """Tránh viết cùng chủ đề liên tiếp bằng cách xem DB."""
    try:
        con = sqlite3.connect(DB_PATH)
        rows = con.execute(
            "SELECT title FROM news ORDER BY created_at DESC LIMIT 12"
        ).fetchall()
        con.close()
        recent_titles = " ".join(r[0] for r in rows).lower()

        # Ưu tiên chủ đề chưa xuất hiện gần đây
        random.shuffle(TOPICS)
        for t in TOPICS:
            if t[0].lower().split()[0] not in recent_titles:
                return t
    except Exception:
        pass
    return random.choice(TOPICS)

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    print(f"\n🤖 auto_news.py — {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")

    topic, category, img_keywords = pick_topic()
    print(f"📝 Chủ đề: {topic} ({category})")

    print("🧠 Đang gọi Gemini...")
    article = generate_article(topic, category)
    print(f"✅ Tiêu đề: {article['title']}")

    image_url = get_image_url(img_keywords)
    print(f"🖼️  Ảnh: {image_url}")

    news_id = save_to_db(article, image_url)
    print(f"💾 Đã lưu DB — ID={news_id}")
    print("🎉 Xong!\n")

if __name__ == "__main__":
    main()
