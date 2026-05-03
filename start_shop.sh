#!/bin/bash
GITHUB_TOKEN=REMOVED_FOR_SECURITY
GEMINI_API_KEY="AIzaSyCZe1zh64PDeWzE_1j43zlUmhEVzQPhDmM"
TUNNEL_REPO="Thooooooo/electroshop-tunnel"
CF_LOG="/tmp/cf_electroshop.log"
PB_PORT=8090

# Hàm kiểm tra cổng bằng Python (thay thế cho nc)
check_port() {
  python3 -c "import socket; s = socket.socket(); s.settimeout(1); exit(0 if s.connect_ex(('127.0.0.1', $1)) == 0 else 1)"
}

echo "🚀 Đang khởi động hệ sinh thái ElectroShop trên Pi 5..."
pkill -f "pocketbase serve" 2>/dev/null || true
nohup /home/tho/pocketbase/pocketbase serve --http="0.0.0.0:$PB_PORT" --dir=/home/tho/pocketbase/pb_data > /tmp/pb.log 2>&1 &
sleep 3

pkill -f "cyd_serial.py" 2>/dev/null || true
nohup python3 /home/tho/electroshop/cyd_serial.py > /tmp/cyd.log 2>&1 &
echo "✅ CYD Serial sẵn sàng"

pkill -f "auto_desc.py" 2>/dev/null || true
GEMINI_API_KEY="$GEMINI_API_KEY" nohup python3 /home/tho/electroshop/auto_desc.py > /tmp/flask.log 2>&1 &
sleep 5

pkill -f "electro_v2.py" 2>/dev/null || true
GEMINI_API_KEY="$GEMINI_API_KEY" nohup python3 /home/tho/electroshop/electro_v2.py > /tmp/electrov2.log 2>&1 &
sleep 5

pkill -f "cloudflared tunnel" 2>/dev/null || true
rm -f "$CF_LOG"
nohup cloudflared tunnel --url "http://localhost:8888" --logfile "$CF_LOG" > /dev/null 2>&1 &
CF_PID=$!

echo "⏳ Đang lấy URL Cloudflare..."
for i in $(seq 1 30); do
  NEW_URL=$(grep -o 'https://[a-zA-Z0-9-]*\.trycloudflare\.com' "$CF_LOG" 2>/dev/null | head -1)
  [ -n "$NEW_URL" ] && break
  sleep 1
done

if [ -n "$NEW_URL" ]; then
  echo "✅ URL mới: $NEW_URL"
  B64_URL=$(echo -n "$NEW_URL" | base64 -w 0)
  CURRENT_SHA=$(curl -s -H "Authorization: token $GITHUB_TOKEN" "https://api.github.com/repos/$TUNNEL_REPO/contents/url.txt" | python3 -c "import sys,json; print(json.load(sys.stdin).get('sha',''))" 2>/dev/null)
  curl -s -X PUT "https://api.github.com/repos/$TUNNEL_REPO/contents/url.txt" -H "Authorization: token $GITHUB_TOKEN" -H "Content-Type: application/json" -d "{\"message\":\"update URL\",\"content\":\"$B64_URL\",\"sha\":\"$CURRENT_SHA\"}" > /dev/null
  echo "🎉 GitHub đã cập nhật!"
fi

echo "✨ Watchdog (Python Mode) đang giám sát..."
while true; do
  sleep 20
  if ! check_port $PB_PORT; then
    echo "⚠️ PocketBase sập — đang restart..."
    nohup /home/tho/pocketbase/pocketbase serve --http="0.0.0.0:$PB_PORT" --dir=/home/tho/pocketbase/pb_data > /tmp/pb.log 2>&1 &
  fi
  if ! check_port 8888; then
    echo "⚠️ Electro_v2 sập — đang restart..."
    nohup python3 /home/tho/electroshop/electro_v2.py > /tmp/electrov2.log 2>&1 &
  fi
done
