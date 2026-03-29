#!/bin/bash
# ============================================================
# update_tunnel.sh - Khởi động Cloudflare tunnel + tự cập nhật
# Worker KV khi URL thay đổi
#
# Cách dùng: ./update_tunnel.sh
# Tự động chạy khi boot: thêm vào crontab hoặc systemd
# ============================================================

set -euo pipefail

LOG="/home/tho/tunnel.log"
TUNNEL_LOG="/tmp/cf_tunnel_8888.log"
KV_NS="833bf6abfe5b4b30bf20f57a19b39667"
ACCOUNT_ID="26716ddfa00429543b50918a42d629ff"
WRANGLER_TOKEN_FILE="$HOME/.wrangler/config/default.toml"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG"; }

# --- Lấy OAuth token từ wrangler config ---
get_token() {
  grep oauth_token "$WRANGLER_TOKEN_FILE" 2>/dev/null | cut -d'"' -f2
}

# --- Cập nhật URL vào Cloudflare KV ---
update_kv() {
  local url="$1"
  local token
  token=$(get_token)
  if [ -z "$token" ]; then
    log "WARN: Không tìm thấy OAuth token, bỏ qua cập nhật KV"
    return 1
  fi
  local result
  result=$(curl -s -X PUT \
    "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/storage/kv/namespaces/$KV_NS/values/PI5_URL" \
    -H "Authorization: Bearer $token" \
    -H "Content-Type: text/plain" \
    --data "$url")
  if echo "$result" | grep -q '"success":true'; then
    log "✅ KV đã cập nhật: $url"
  else
    log "❌ Lỗi KV: $result"
  fi
}

# --- Dừng tunnel cũ nếu còn chạy ---
OLD_PID=$(pgrep -f "cloudflared tunnel --url http://localhost:8888" 2>/dev/null | head -1 || true)
if [ -n "$OLD_PID" ]; then
  log "Dừng tunnel cũ PID=$OLD_PID"
  kill "$OLD_PID" 2>/dev/null || true
  sleep 2
fi

# --- Khởi động tunnel mới ---
log "Khởi động Cloudflare tunnel cho port 8888..."
rm -f "$TUNNEL_LOG"
cloudflared tunnel --url http://localhost:8888 > "$TUNNEL_LOG" 2>&1 &
TUNNEL_PID=$!
log "Tunnel PID=$TUNNEL_PID, đang chờ URL..."

# --- Chờ lấy URL (tối đa 30 giây) ---
TUNNEL_URL=""
for i in $(seq 1 30); do
  sleep 1
  TUNNEL_URL=$(grep -oP 'https://[a-zA-Z0-9-]+\.trycloudflare\.com' "$TUNNEL_LOG" 2>/dev/null | tail -1 || true)
  if [ -n "$TUNNEL_URL" ]; then
    break
  fi
done

if [ -z "$TUNNEL_URL" ]; then
  log "❌ Không lấy được URL tunnel sau 30s"
  exit 1
fi

log "🔗 Tunnel URL mới: $TUNNEL_URL"

# --- Cập nhật Worker KV ---
update_kv "$TUNNEL_URL"

log "🚀 Xong! Worker cố định: https://electroshop-pi5.electroshop-tho.workers.dev"
log "   Tunnel thực tế:       $TUNNEL_URL"
