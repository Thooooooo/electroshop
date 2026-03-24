#!/bin/bash

# Thư mục chứa code hiện tại
WEB_DIR="/home/tho/electroshop" 

echo "🚀 Bật Tunnel cổng 8080..."
nohup cloudflared tunnel --url http://localhost:8090 > /tmp/cf_log.txt 2>&1 &

echo "⏳ Đợi 5 giây cho Cloudflare tạo link..."
sleep 5

NEW_LINK=$(grep -o 'https://[a-zA-Z0-9-]*\.trycloudflare\.com' /tmp/cf_log.txt | head -n 1)

if [ -n "$NEW_LINK" ]; then
    echo "✅ Link mới: $NEW_LINK"
    # TUYỆT KỸ: Ghi link vào api.js
    echo "const CLOUD_LINK = '$NEW_LINK';" > $WEB_DIR/api.js
    echo "🎉 Đã cập nhật file api.js thành công!"
else
    echo "❌ Lỗi: Không lấy được link."
fi
