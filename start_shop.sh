#!/bin/bash
# start_shop.sh — Khởi động ElectroShop + Cloudflare Tunnel
# Tự động cập nhật URL mới lên GitHub → Vercel nhận ngay, không cần redeploy

set -e

GITHUB_TOKEN="${GITHUB_TOKEN:-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}"
TUNNEL_REPO="Thooooooo/electroshop-tunnel"
CF_LOG="/tmp/cf_electroshop.log"
PB_PORT=8090

echo "🚀 Khởi động PocketBase (cổng $PB_PORT)..."
pkill -f "pocketbase serve" 2>/dev/null || true
nohup /home/tho/pocketbase/pocketbase serve --http="0.0.0.0:$PB_PORT" \
  --dir=/home/tho/pocketbase/pb_data > /tmp/pb.log 2>&1 &
sleep 2

echo "☁️  Khởi động Cloudflare Tunnel..."
pkill -f "cloudflared tunnel" 2>/dev/null || true
rm -f "$CF_LOG"
nohup cloudflared tunnel --url "http://localhost:$PB_PORT" \
  --logfile "$CF_LOG" > /dev/null 2>&1 &
CF_PID=$!

echo "⏳ Chờ Cloudflare tạo URL..."
for i in $(seq 1 30); do
  NEW_URL=$(grep -o 'https://[a-zA-Z0-9-]*\.trycloudflare\.com' "$CF_LOG" 2>/dev/null | head -1)
  [ -n "$NEW_URL" ] && break
  sleep 1
done

if [ -z "$NEW_URL" ]; then
  echo "❌ Không lấy được URL sau 30 giây. Kiểm tra log: $CF_LOG"
  exit 1
fi

echo "✅ URL mới: $NEW_URL"

# --- Cập nhật url.txt trên GitHub (không cần git, chỉ cần 1 API call) ---
echo "📡 Đang đẩy URL lên GitHub..."

# Lấy SHA hiện tại của file (cần để update)
CURRENT_SHA=$(curl -s \
  -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/$TUNNEL_REPO/contents/url.txt" \
  | python3 -c "import sys,json; print(json.load(sys.stdin).get('sha',''))" 2>/dev/null)

# Encode URL thành base64
B64_URL=$(echo -n "$NEW_URL" | base64 -w 0)

# Push lên GitHub
HTTP_CODE=$(curl -s -o /tmp/gh_push.json -w "%{http_code}" \
  -X PUT "https://api.github.com/repos/$TUNNEL_REPO/contents/url.txt" \
  -H "Authorization: token $GITHUB_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"message\":\"tunnel: update URL $(date '+%H:%M:%S')\",\"content\":\"$B64_URL\",\"sha\":\"$CURRENT_SHA\"}")

if [ "$HTTP_CODE" = "200" ] || [ "$HTTP_CODE" = "201" ]; then
  echo "🎉 GitHub cập nhật thành công! Web sẽ dùng URL mới ngay lập tức."
  echo "🌐 Vercel: https://electroshop-ten.vercel.app"
else
  echo "⚠️  GitHub push thất bại (HTTP $HTTP_CODE). Chi tiết: $(cat /tmp/gh_push.json)"
fi

echo ""
echo "📊 Trạng thái:"
echo "   PocketBase : http://localhost:$PB_PORT"
echo "   Tunnel URL : $NEW_URL"
echo "   Vercel     : https://electroshop-ten.vercel.app"
echo ""
echo "✨ ElectroShop đang chạy! (Ctrl+C để dừng tunnel)"
wait $CF_PID

