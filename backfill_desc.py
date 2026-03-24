#!/usr/bin/env python3
"""
backfill_desc.py — Tự động điền mô tả sản phẩm bằng Gemini AI
cho tất cả sản phẩm trong PocketBase đang bị trống trường 'mo_ta'.

Cách dùng:
    export GEMINI_API_KEY="your_key"
    python3 backfill_desc.py

    # Chỉ xem (dry-run, không ghi):
    python3 backfill_desc.py --dry-run
"""

import os, sys, time, argparse, requests

PB_URL   = "http://127.0.0.1:8090"
AI_URL   = "http://127.0.0.1:5001/generate"

# ── Lấy danh sách sản phẩm cần fill ──────────────────────────
def fetch_empty(token=None):
    headers = {"Authorization": f"Bearer {token}"} if token else {}
    all_items, page = [], 1
    while True:
        r = requests.get(f"{PB_URL}/api/collections/sanpham/records",
                         params={"perPage": 100, "page": page}, headers=headers, timeout=10)
        r.raise_for_status()
        data = r.json()
        items = data.get("items", [])
        for p in items:
            if not (p.get("mo_ta") or "").strip():
                all_items.append(p)
        if page >= data.get("totalPages", 1):
            break
        page += 1
    return all_items

# ── Sinh mô tả từ Flask/Gemini (auto_desc.py) ────────────────
def generate_desc(ten: str, gia=None) -> str:
    payload = {"ten": ten}
    if gia:
        payload["gia"] = gia
    r = requests.post(AI_URL, json=payload, timeout=30)
    r.raise_for_status()
    return r.json().get("mo_ta", "")

# ── Cập nhật PocketBase ───────────────────────────────────────
def update_product(pid: str, mo_ta: str, token=None):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    r = requests.patch(f"{PB_URL}/api/collections/sanpham/records/{pid}",
                       json={"mo_ta": mo_ta}, headers=headers, timeout=10)
    r.raise_for_status()

# ── PocketBase admin login ─────────────────────────────────────
def pb_login(email, password):
    r = requests.post(f"{PB_URL}/api/admins/auth-with-password",
                      json={"identity": email, "password": password}, timeout=10)
    if r.ok:
        return r.json().get("token")
    return None

# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true", help="Chỉ xem, không ghi")
    parser.add_argument("--email",   default=os.environ.get("PB_EMAIL", ""))
    parser.add_argument("--password",default=os.environ.get("PB_PASSWORD", ""))
    args = parser.parse_args()

    # Đăng nhập PocketBase (nếu có)
    token = None
    if args.email and args.password:
        token = pb_login(args.email, args.password)
        print(f"[PB] Đăng nhập {'OK' if token else 'THẤT BẠI'}")

    # Kiểm tra Flask/Gemini đang chạy
    try:
        requests.get("http://127.0.0.1:5001/health", timeout=3).raise_for_status()
    except Exception:
        print("❌ Flask server (port 5001) không phản hồi!")
        print("   Chạy trước: nohup env GEMINI_API_KEY=... python3 auto_desc.py &")
        sys.exit(1)

    # Lấy danh sách cần fill
    empty = fetch_empty(token)
    total = len(empty)

    if total == 0:
        print("✅ Tất cả sản phẩm đã có mô tả rồi!")
        return

    print(f"📋 Tìm thấy {total} sản phẩm chưa có mô tả:")
    for p in empty:
        print(f"   [{p['id']}] {p.get('ten','')}")

    if args.dry_run:
        print("\n[DRY-RUN] Không ghi gì. Dùng bỏ --dry-run để chạy thật.")
        return

    print(f"\n🤖 Bắt đầu sinh mô tả bằng Gemini AI...\n")
    ok, fail = 0, 0

    for i, p in enumerate(empty, 1):
        ten = p.get("ten", "")
        gia = p.get("gia")
        pid = p["id"]
        print(f"[{i}/{total}] {ten[:40]}...", end=" ", flush=True)
        try:
            mo_ta = generate_desc(ten, gia)
            if not mo_ta.strip():
                raise ValueError("Gemini trả về chuỗi rỗng")
            update_product(pid, mo_ta, token)
            print(f"✅ ({len(mo_ta)} ký tự)")
            ok += 1
        except Exception as e:
            print(f"❌ {e}")
            fail += 1
        # Tránh rate-limit Gemini free tier
        if i < total:
            time.sleep(2)

    print(f"\n{'='*45}")
    print(f"✅ Thành công: {ok}/{total} sản phẩm đã được điền mô tả.")
    if fail:
        print(f"❌ Thất bại:   {fail} sản phẩm (chạy lại để thử lại).")

if __name__ == "__main__":
    main()
