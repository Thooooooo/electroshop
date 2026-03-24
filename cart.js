/**
 * cart.js — Giỏ hàng dùng chung cho ElectroShop
 * Include trước </body> trên mọi trang.
 */
const CART_KEY = 'electroshop_cart';

/* ── Helpers lưu/đọc ─────────────────────────────────────── */
function cartGet() {
  try { return JSON.parse(localStorage.getItem(CART_KEY)) || []; }
  catch { return []; }
}
function cartSave(items) {
  localStorage.setItem(CART_KEY, JSON.stringify(items));
  cartBadgeUpdate();
}

/* ── Chuyển chuỗi giá → số ─────────────────────────────── */
function parsePrice(p) {
  if (typeof p === 'number') return p;
  return parseInt(String(p).replace(/[^\d]/g, '')) || 0;
}

/* ── API công khai ──────────────────────────────────────── */
function cartAdd(product) {
  // product: { id, name, price (string|number), icon }
  const items = cartGet();
  const idx   = items.findIndex(i => String(i.id) === String(product.id));
  if (idx >= 0) {
    items[idx].qty += 1;
  } else {
    items.push({
      id:    String(product.id),
      name:  product.name  || 'Sản phẩm',
      icon:  product.icon  || '🛒',
      price: parsePrice(product.price),
      qty:   1,
    });
  }
  cartSave(items);
  cartToast(product.name);
}

function cartRemove(id) {
  cartSave(cartGet().filter(i => String(i.id) !== String(id)));
}

function cartUpdate(id, qty) {
  const items = cartGet();
  const idx   = items.findIndex(i => String(i.id) === String(id));
  if (idx < 0) return;
  if (qty <= 0) { items.splice(idx, 1); }
  else          { items[idx].qty = qty; }
  cartSave(items);
}

function cartCount() {
  return cartGet().reduce((s, i) => s + i.qty, 0);
}

function cartTotal() {
  return cartGet().reduce((s, i) => s + i.price * i.qty, 0);
}

function cartClear() {
  localStorage.removeItem(CART_KEY);
  cartBadgeUpdate();
}

/* ── Badge số lượng trên icon giỏ ──────────────────────── */
function cartBadgeUpdate() {
  const n = cartCount();
  document.querySelectorAll('.cart-badge').forEach(el => {
    el.textContent = n;
    el.style.display = n > 0 ? 'flex' : 'none';
  });
}

/* ── Toast thông báo thêm vào giỏ ──────────────────────── */
function cartToast(name) {
  let t = document.getElementById('cartToast');
  if (!t) {
    t = document.createElement('div');
    t.id = 'cartToast';
    t.style.cssText = `
      position:fixed;bottom:24px;right:24px;z-index:9999;
      background:#2563eb;color:#fff;padding:12px 20px;border-radius:12px;
      font-size:.95rem;box-shadow:0 4px 20px rgba(37,99,235,.4);
      transform:translateY(80px);opacity:0;
      transition:.3s cubic-bezier(.4,0,.2,1);pointer-events:none;
    `;
    document.body.appendChild(t);
  }
  t.textContent = `🛒 Đã thêm: ${(name||'').slice(0,30)}`;
  t.style.transform = 'translateY(0)';
  t.style.opacity   = '1';
  clearTimeout(t._timer);
  t._timer = setTimeout(() => {
    t.style.transform = 'translateY(80px)';
    t.style.opacity   = '0';
  }, 2500);
}

/* ── Khởi tạo khi DOM sẵn ──────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {
  // Inject badge vào mọi nút giỏ hàng
  document.querySelectorAll('[data-cart-btn], .btn-icon[title="Giỏ hàng"]').forEach(btn => {
    btn.style.position = 'relative';
    btn.onclick = () => { window.location.href = 'cart.html'; };
    if (!btn.querySelector('.cart-badge')) {
      const badge = document.createElement('span');
      badge.className = 'cart-badge';
      badge.style.cssText = `
        position:absolute;top:-6px;right:-6px;
        background:#ef4444;color:#fff;border-radius:50%;
        width:18px;height:18px;font-size:11px;font-weight:700;
        display:none;align-items:center;justify-content:center;
        line-height:1;box-shadow:0 2px 6px rgba(239,68,68,.5);
      `;
      btn.appendChild(badge);
    }
  });
  cartBadgeUpdate();
});
