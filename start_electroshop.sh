#!/bin/bash

# ============================================================

# ElectroShop - Auto Start Script cho Raspberry Pi

# Chay Flask + Cloudflare tunnel, tu update URL len GitHub

# 

# HUONG DAN:

# 1. export GITHUB_TOKEN=ghp_xxxxxxxxxx

# 2. chmod +x start_electroshop.sh

# 3. ./start_electroshop.sh

# ============================================================

# CAU HINH - SUA O DAY

GITHUB_REPO=“Thooooooo/electroshop-tunnel”
GITHUB_FILE=“url.txt”
FLASK_SCRIPT=”/home/tho/electroshop/electro_v4.py”
FLASK_PORT=8888
LOG_FILE=”/home/tho/electroshop/tunnel.log”

# Kiem tra GITHUB_TOKEN da export chua

if [ -z “$GITHUB_TOKEN” ]; then
echo “Chua co GITHUB_TOKEN! Chay lenh nay truoc:”
echo “  export GITHUB_TOKEN=ghp_xxxxxxxxxx”
exit 1
fi

echo “==> Khoi dong ElectroShop…”

# Dung process cu

pkill -f “electro_v4.py” 2>/dev/null
pkill -f “cloudflared” 2>/dev/null
sleep 2

# Khoi dong Flask

echo “==> Khoi dong Flask…”
python3 “$FLASK_SCRIPT” > /tmp/flask.log 2>&1 &
FLASK_PID=$!
sleep 3

if ! kill -0 $FLASK_PID 2>/dev/null; then
echo “FAIL: Flask khong chay duoc! Xem: /tmp/flask.log”
exit 1
fi
echo “  OK: Flask dang chay port $FLASK_PORT”

# Khoi dong Cloudflare tunnel

echo “==> Khoi dong Cloudflare tunnel…”
cloudflared tunnel –url “http://localhost:$FLASK_PORT”   
–no-autoupdate   
–logfile “$LOG_FILE”   
–loglevel info   
> /tmp/cf_output.log 2>&1 &
CF_PID=$!

# Cho lay URL (toi da 30 giay)

echo “  Dang cho URL tunnel…”
TUNNEL_URL=””
for i in $(seq 1 30); do
sleep 1
TUNNEL_URL=$(grep -oP ‘https://[a-z0-9-]+.trycloudflare.com’ “$LOG_FILE” 2>/dev/null | head -1)
if [ -n “$TUNNEL_URL” ]; then
break
fi
done

if [ -z “$TUNNEL_URL” ]; then
echo “FAIL: Khong lay duoc URL! Xem: $LOG_FILE”
exit 1
fi
echo “  OK: URL = $TUNNEL_URL”

# Cap nhat URL len GitHub

echo “==> Cap nhat GitHub…”
GITHUB_API=“https://api.github.com/repos/$GITHUB_REPO/contents/$GITHUB_FILE”

FILE_SHA=$(curl -s   
-H “Authorization: token $GITHUB_TOKEN”   
-H “User-Agent: ElectroShop-Pi”   
“$GITHUB_API” | python3 -c “import sys,json; d=json.load(sys.stdin); print(d.get(‘sha’,’’))” 2>/dev/null)

URL_BASE64=$(echo -n “$TUNNEL_URL” | base64 -w 0)

if [ -n “$FILE_SHA” ]; then
PAYLOAD=”{"message":"Auto-update tunnel URL","content":"$URL_BASE64","sha":"$FILE_SHA"}”
else
PAYLOAD=”{"message":"Create tunnel URL","content":"$URL_BASE64"}”
fi

RESULT=$(curl -s -X PUT   
-H “Authorization: token $GITHUB_TOKEN”   
-H “Content-Type: application/json”   
-H “User-Agent: ElectroShop-Pi”   
-d “$PAYLOAD”   
“$GITHUB_API”)

if echo “$RESULT” | grep -q ‘“content”’; then
echo “  OK: GitHub da cap nhat thanh cong!”
echo “”
echo “========================================”
echo “  ElectroShop dang chay!”
echo “  Flask   : http://localhost:$FLASK_PORT”
echo “  Tunnel  : $TUNNEL_URL”
echo “  Web Vercel se tu lay URL moi.”
echo “========================================”
else
echo “  WARN: Cap nhat GitHub that bai:”
echo “$RESULT” | python3 -c “import sys,json; d=json.load(sys.stdin); print(d.get(‘message’,’?’))” 2>/dev/null
fi

wait $CF_PID
