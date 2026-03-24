#!/usr/bin/env python3
"""
fill_missing_data.py — Điền mo_ta + loai còn trống bằng Gemini AI
Đồng thời hiện tên sản phẩm đang xử lý lên màn hình ST7735.

Dùng:
    python3 fill_missing_data.py
    python3 fill_missing_data.py --dry-run
"""
import os, sys, time, argparse, requests

PB_URL  = "http://127.0.0.1:8090"
AI_URL  = "http://127.0.0.1:5001/generate"
MON_URL = "http://127.0.0.1:5001/monitor-notify"

PB_EMAIL    = os.environ.get("PB_EMAIL",    "macducthohd@gmail.com")
PB_PASSWORD = os.environ.get("PB_PASSWORD", "ductho2011")

LOAI_MAP = {
    "iphone":"dien-thoai","samsung":"dien-thoai","xiaomi":"dien-thoai",
    "macbook":"may-tinh","laptop":"may-tinh","pc":"may-tinh","luckfox":"may-tinh-nhung","raspberry":"may-tinh-nhung","arduino":"may-tinh-nhung","esp32":"may-tinh-nhung",
    "watch":"dong-ho","airpods":"am-thanh","speaker":"am-thanh","loa":"am-thanh",
    "ipad":"may-tinh-bang","playstation":"game","ps5":"game",
}

def guess_loai(ten: str) -> str:
    t = ten.lower()
    for k, v in LOAI_MAP.items():
        if k in t: return v
    return "dien-tu"

def pb_login():
    r = requests.post(f"{PB_URL}/api/admins/auth-with-password",
                      json={"identity": PB_EMAIL, "password": PB_PASSWORD}, timeout=10)
    return r.json().get("token") if r.ok else None

def fetch_all(token):
    headers = {"Authorization": f"Bearer {token}"} if token else {}
    items, page = [], 1
    while True:
        r = requests.get(f"{PB_URL}/api/collections/sanpham/records",
                         params={"perPage":100,"page":page}, headers=headers, timeout=10)
        r.raise_for_status()
        d = r.json()
        items += d.get("items", [])
        if page >= d.get("totalPages", 1): break
        page += 1
    return items

def needs_fill(p):
    return not (p.get("mo_ta") or "").strip() or not (p.get("loai") or "").strip()

def generate_desc(ten, gia=None):
    r = requests.post(AI_URL, json={"ten": ten, "gia": gia}, timeout=30)
    r.raise_for_status()
    return r.json().get("mo_ta", "")

def monitor_notify(ten, done=False):
    try:
        requests.post(MON_URL,
                      json={"event": "done" if done else "start", "product": ten},
                      timeout=2)
    except: pass

def update_pb(pid, data, token):
    headers = {"Content-Type": "application/json"}
    if token: headers["Authorization"] = f"Bearer {token}"
    r = requests.patch(f"{PB_URL}/api/collections/sanpham/records/{pid}",
                       json=data, headers=headers, timeout=10)
    r.raise_for_status()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    # Kiểm tra Flask AI
    try:
        requests.get("http://127.0.0.1:5001/health", timeout=3).raise_for_status()
    except:
        print("❌ Flask server (port 5001) chưa chạy!")
        sys.exit(1)

    token = pb_login()
    print(f"[PB] Đăng nhập: {'OK' if token else 'THẤT BẠI — thử không có auth'}")

    all_products = fetch_all(token)
    to_fill = [p for p in all_products if needs_fill(p)]
    total   = len(to_fill)

    print(f"\n📦 Tổng: {len(all_products)} SP | Cần fill: {total} SP")
    if total == 0:
        print("✅ Tất cả đã đủ dữ liệu!"); return
    for p in to_fill:
        flag = []
        if not (p.get("mo_ta") or "").strip(): flag.append("mo_ta")
        if not (p.get("loai") or "").strip():  flag.append("loai")
        print(f"  [{p['id']}] {p.get('ten','')[:35]} → thiếu: {', '.join(flag)}")

    if args.dry_run:
        print("\n[DRY-RUN] Không ghi. Bỏ --dry-run để chạy thật."); return

    print(f"\n🤖 Bắt đầu fill...\n")
    ok = fail = 0

    for i, p in enumerate(to_fill, 1):
        ten = p.get("ten", "")
        pid = p["id"]
        update = {}

        print(f"[{i}/{total}] {ten[:40]}", end=" ", flush=True)
        monitor_notify(ten)   # Hiện lên ST7735

        # Fill loai
        if not (p.get("loai") or "").strip():
            update["loai"] = guess_loai(ten)
            print(f"(loai={update['loai']})", end=" ", flush=True)

        # Fill mo_ta
        if not (p.get("mo_ta") or "").strip():
            try:
                desc = generate_desc(ten, p.get("gia"))
                if not desc.strip(): raise ValueError("Gemini rỗng")
                update["mo_ta"] = desc
                print(f"(mo_ta={len(desc)}c)", end=" ", flush=True)
            except Exception as e:
                print(f"❌ AI lỗi: {e}")
                fail += 1
                monitor_notify(ten, done=True)
                continue

        try:
            update_pb(pid, update, token)
            print("✅")
            ok += 1
        except Exception as e:
            print(f"❌ PB lỗi: {e}")
            fail += 1

        monitor_notify(ten, done=True)   # Xong → clear ST7735
        if i < total: time.sleep(2)      # Tránh rate-limit

    print(f"\n{'='*45}")
    print(f"✅ Thành công: {ok}/{total}")
    if fail: print(f"❌ Thất bại:  {fail} (chạy lại để thử)")

if __name__ == "__main__":
    main()
