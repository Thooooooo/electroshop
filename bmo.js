/* ============================================================
 * bmo.js — BMO Chatbot Widget v1.0
 * Nhân vật BMO (Adventure Time) – tư vấn mua hàng ElectroShop
 * ============================================================ */
(function () {
  'use strict';

  /* ── Kiến thức BMO ── */
  const KNOWLEDGE = [
    { keys: ['xin chào','hello','hi','chào','hey','alo'], reply: '🌈 Bờm bờm! Mình là **BMO** – trợ lý mua sắm của ElectroShop! Mình có thể giúp bạn tìm sản phẩm, hỏi giá, tra giỏ hàng,… Hỏi mình đi nào! ✨' },
    { keys: ['sản phẩm','mua gì','bán gì','hàng','đồ gì'], reply: '🛍️ ElectroShop bán các thiết bị điện tử chính hãng: điện thoại, tai nghe, laptop, phụ kiện,… Bạn xem tại <a href="chitiet.html" style="color:#2563eb;font-weight:700;">trang Sản phẩm</a> nhé!' },
    { keys: ['giá','bao nhiêu','rẻ','đắt','cost','price'], reply: '💰 Giá sản phẩm được cập nhật trực tiếp từ kho. Bạn click vào từng sản phẩm để xem giá chi tiết. Thường xuyên có **Flash Sale giảm đến 50%** đấy!' },
    { keys: ['giỏ hàng','cart','thêm vào','basket'], reply: '🛒 Giỏ hàng của bạn có thể xem tại <a href="cart.html" style="color:#2563eb;font-weight:700;">đây</a>. Nhấn 🛒 trên góc trên cùng cũng được nha!' },
    { keys: ['thanh toán','checkout','đặt hàng','order','mua'], reply: '💳 Sau khi chọn hàng xong, vào <a href="cart.html" style="color:#2563eb;font-weight:700;">Giỏ hàng</a> → nhấn **Thanh toán**. Hỗ trợ chuyển khoản, ví MoMo, ZaloPay!' },
    { keys: ['giao hàng','ship','vận chuyển','nhận hàng','delivery'], reply: '🚚 Giao hàng **nội thành 2 giờ**, **toàn quốc 1–3 ngày**. Miễn phí giao hàng cho đơn từ **500.000₫**!' },
    { keys: ['bảo hành','warranty','hư','lỗi','sửa'], reply: '🛡️ Tất cả sản phẩm được bảo hành **12–24 tháng** tại trung tâm bảo hành chính hãng. Đổi mới trong 7 ngày nếu lỗi nhà sản xuất!' },
    { keys: ['tin tức','báo','news','bài viết','thông tin'], reply: '📰 Xem tin tức công nghệ mới nhất tại <a href="news.html" style="color:#2563eb;font-weight:700;">trang Báo</a> nhé! Cập nhật hàng ngày!' },
    { keys: ['liên hệ','hotline','số điện thoại','email','contact'], reply: '📞 Liên hệ ElectroShop:\n• Hotline: **1900-xxxx**\n• Email: support@electroshop.vn\n• Giờ làm việc: 8:00–22:00 hàng ngày' },
    { keys: ['trả hàng','hoàn tiền','refund','return'], reply: '↩️ Chính sách hoàn trả trong **30 ngày** nếu hàng lỗi. Liên hệ hotline để được hỗ trợ nhanh nhất!' },
    { keys: ['khuyến mãi','giảm giá','sale','deal','voucher','coupon'], reply: '🔥 Flash Sale diễn ra hàng tuần! Theo dõi <a href="news.html" style="color:#2563eb;font-weight:700;">Tin tức</a> để không bỏ lỡ các deal hot nhé!' },
    { keys: ['cảm ơn','thanks','thank you','tuyệt','hay','ok'], reply: '💚 Cảm ơn bạn! BMO rất vui khi được giúp đỡ. Chúc bạn mua sắm vui vẻ tại ElectroShop! 🎮✨' },
    { keys: ['bmo','mày là ai','bạn là ai','robot','ai đó'], reply: '🎮 Mình là **BMO** – một chiếc máy chơi game biết nói chuyện từ xứ Ooo! Bây giờ mình đang làm trợ lý tại ElectroShop. Finn và Jake đã cho mình mượn! 😄' },
  ];

  const DEFAULT_REPLY = '🤔 BMO chưa hiểu câu đó lắm! Bạn có thể hỏi về: **sản phẩm**, **giá cả**, **giao hàng**, **bảo hành**, hay **khuyến mãi** không? Mình sẵn sàng giúp! 🌈';

  function getReply(msg) {
    const m = msg.toLowerCase().normalize('NFC');
    for (const { keys, reply } of KNOWLEDGE) {
      if (keys.some(k => m.includes(k))) return reply;
    }
    return DEFAULT_REPLY;
  }

  /* ── Tạo DOM ── */
  const style = document.createElement('style');
  style.textContent = `
    #bmo-btn {
      position: fixed; bottom: 28px; right: 28px; z-index: 9999;
      width: 64px; height: 64px; border-radius: 50%; border: none;
      background: linear-gradient(135deg, #4ade80, #22c55e);
      box-shadow: 0 6px 24px rgba(34,197,94,.45);
      cursor: pointer; font-size: 2rem;
      display: flex; align-items: center; justify-content: center;
      transition: transform .2s, box-shadow .2s;
      animation: bmo-bounce 2.5s ease-in-out infinite;
    }
    #bmo-btn:hover { transform: scale(1.12); box-shadow: 0 10px 32px rgba(34,197,94,.6); }
    @keyframes bmo-bounce { 0%,100%{transform:translateY(0)} 50%{transform:translateY(-6px)} }
    #bmo-btn.open { animation: none; transform: rotate(10deg) scale(1.05); }

    #bmo-window {
      position: fixed; bottom: 108px; right: 28px; z-index: 9998;
      width: 340px; max-height: 520px;
      background: #fff; border-radius: 20px;
      box-shadow: 0 16px 56px rgba(0,0,0,.22);
      display: flex; flex-direction: column; overflow: hidden;
      transform: scale(0); transform-origin: bottom right;
      transition: transform .25s cubic-bezier(.34,1.56,.64,1), opacity .2s;
      opacity: 0; pointer-events: none;
    }
    [data-theme="dark"] #bmo-window { background: #1e293b; color: #f1f5f9; }
    #bmo-window.open { transform: scale(1); opacity: 1; pointer-events: all; }

    #bmo-head {
      background: linear-gradient(135deg, #4ade80, #22c55e);
      padding: 14px 18px; display: flex; align-items: center; gap: 12px;
    }
    #bmo-avatar {
      width: 44px; height: 44px; border-radius: 10px;
      background: #86efac; display: flex; align-items: center;
      justify-content: center; font-size: 1.6rem;
      flex-shrink: 0; box-shadow: 0 2px 8px rgba(0,0,0,.15);
    }
    #bmo-title { color: #fff; }
    #bmo-title strong { display: block; font-weight: 800; font-size: 1rem; }
    #bmo-title span { font-size: .75rem; opacity: .85; }
    #bmo-close {
      margin-left: auto; background: rgba(255,255,255,.3); border: none;
      border-radius: 50%; width: 28px; height: 28px; cursor: pointer;
      color: #fff; font-size: 1rem; display: flex; align-items: center; justify-content: center;
      transition: background .15s;
    }
    #bmo-close:hover { background: rgba(255,255,255,.5); }

    #bmo-messages {
      flex: 1; overflow-y: auto; padding: 16px;
      display: flex; flex-direction: column; gap: 10px;
      min-height: 200px;
    }
    #bmo-messages::-webkit-scrollbar { width: 4px; }
    #bmo-messages::-webkit-scrollbar-thumb { background: #cbd5e1; border-radius: 2px; }

    .bmo-msg {
      max-width: 85%; padding: 10px 14px; border-radius: 14px;
      font-size: .875rem; line-height: 1.55; word-wrap: break-word;
    }
    .bmo-msg.bot {
      background: #f0fdf4; color: #166534; border-bottom-left-radius: 4px;
      align-self: flex-start; border: 1px solid #bbf7d0;
    }
    [data-theme="dark"] .bmo-msg.bot { background: #14532d; color: #bbf7d0; border-color: #166534; }
    .bmo-msg.user {
      background: linear-gradient(135deg, #2563eb, #7c3aed);
      color: #fff; border-bottom-right-radius: 4px;
      align-self: flex-end;
    }
    .bmo-msg a { color: inherit; text-decoration: underline; }

    #bmo-quick {
      padding: 8px 16px 4px; display: flex; gap: 6px; flex-wrap: wrap;
    }
    .bmo-quick-btn {
      padding: 5px 12px; border-radius: 999px;
      border: 1.5px solid #4ade80; background: transparent;
      color: #16a34a; font-size: .75rem; font-weight: 600;
      cursor: pointer; transition: all .15s; white-space: nowrap;
    }
    [data-theme="dark"] .bmo-quick-btn { color: #4ade80; border-color: #4ade80; }
    .bmo-quick-btn:hover { background: #4ade80; color: #fff; }

    #bmo-form {
      padding: 12px 16px; border-top: 1px solid #e2e8f0; display: flex; gap: 8px;
    }
    [data-theme="dark"] #bmo-form { border-color: #334155; }
    #bmo-input {
      flex: 1; padding: 10px 14px; border-radius: 999px;
      border: 1.5px solid #e2e8f0; background: #f8fafc; color: #1e293b;
      font-size: .875rem; outline: none; transition: border .2s;
    }
    [data-theme="dark"] #bmo-input { background: #0f172a; color: #f1f5f9; border-color: #334155; }
    #bmo-input:focus { border-color: #4ade80; }
    #bmo-send {
      width: 40px; height: 40px; border-radius: 50%; border: none;
      background: linear-gradient(135deg, #4ade80, #22c55e);
      color: #fff; font-size: 1.1rem; cursor: pointer;
      display: flex; align-items: center; justify-content: center;
      transition: opacity .2s;
    }
    #bmo-send:hover { opacity: .85; }

    @media (max-width: 400px) {
      #bmo-window { width: calc(100vw - 24px); right: 12px; }
      #bmo-btn { bottom: 16px; right: 16px; }
    }
  `;
  document.head.appendChild(style);

  const btn = document.createElement('button');
  btn.id = 'bmo-btn';
  btn.title = 'Chat với BMO';
  btn.innerHTML = '🎮';

  const win = document.createElement('div');
  win.id = 'bmo-window';
  win.innerHTML = `
    <div id="bmo-head">
      <div id="bmo-avatar">🎮</div>
      <div id="bmo-title">
        <strong>BMO</strong>
        <span>Trợ lý ElectroShop • Sẵn sàng giúp!</span>
      </div>
      <button id="bmo-close">✕</button>
    </div>
    <div id="bmo-messages"></div>
    <div id="bmo-quick">
      <button class="bmo-quick-btn" data-msg="Xem sản phẩm">🛍️ Sản phẩm</button>
      <button class="bmo-quick-btn" data-msg="Chính sách giao hàng">🚚 Giao hàng</button>
      <button class="bmo-quick-btn" data-msg="Bảo hành">🛡️ Bảo hành</button>
      <button class="bmo-quick-btn" data-msg="Khuyến mãi">🔥 Sale</button>
    </div>
    <form id="bmo-form">
      <input id="bmo-input" type="text" placeholder="Nhập câu hỏi…" autocomplete="off" />
      <button type="submit" id="bmo-send">➤</button>
    </form>`;

  document.body.appendChild(btn);
  document.body.appendChild(win);

  /* ── Logic ── */
  const msgs = win.querySelector('#bmo-messages');

  function addMsg(text, type) {
    const d = document.createElement('div');
    d.className = `bmo-msg ${type}`;
    d.innerHTML = text
      .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
      .replace(/\n/g, '<br>');
    msgs.appendChild(d);
    msgs.scrollTop = msgs.scrollHeight;
  }

  function greet() {
    addMsg('🌈 Xin chào! Mình là **BMO** – trợ lý của ElectroShop! 🎮\nBạn cần giúp gì nào?', 'bot');
  }

  let opened = false;
  btn.addEventListener('click', () => {
    const isOpen = win.classList.toggle('open');
    btn.classList.toggle('open', isOpen);
    if (isOpen && !opened) { opened = true; greet(); }
    if (isOpen) win.querySelector('#bmo-input').focus();
  });

  win.querySelector('#bmo-close').addEventListener('click', () => {
    win.classList.remove('open'); btn.classList.remove('open');
  });

  win.querySelector('#bmo-form').addEventListener('submit', e => {
    e.preventDefault();
    const inp = win.querySelector('#bmo-input');
    const msg = inp.value.trim();
    if (!msg) return;
    addMsg(msg, 'user');
    inp.value = '';
    setTimeout(() => addMsg(getReply(msg), 'bot'), 400);
  });

  win.querySelectorAll('.bmo-quick-btn').forEach(b => {
    b.addEventListener('click', () => {
      const msg = b.dataset.msg;
      addMsg(msg, 'user');
      setTimeout(() => addMsg(getReply(msg), 'bot'), 350);
    });
  });
})();
