/* ============================================================
 * api.js — ElectroShop shared API module
 * Tất cả các trang đều dùng window.API để gọi PocketBase/Flask
 * API_URL được lấy từ /api/config (Vercel env: API_URL)
 * ============================================================ */
(function () {
  'use strict';

  let _cached = null;   // cache URL sau lần fetch đầu

  /* Lấy base URL từ Vercel env (qua serverless function /api/config) */
  async function getApiUrl() {
    if (_cached !== null) return _cached;
    try {
      const res = await fetch('/api/config');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      _cached = (data.apiUrl || '').replace(/\/$/, '');
    } catch (e) {
      console.warn('[API] getApiUrl failed:', e.message);
      _cached = '';
    }
    return _cached;
  }

  /* Xây URL file ảnh PocketBase */
  function fileUrl(base, collectionId, recordId, filename, thumb) {
    const url = `${base}/api/files/${collectionId}/${recordId}/${filename}`;
    return thumb ? `${url}?thumb=${thumb}` : url;
  }

  /* Lấy 1 sản phẩm theo id */
  async function fetchProduct(id) {
    const base = await getApiUrl();
    if (!base) return null;
    const res = await fetch(`${base}/api/collections/sanpham/records/${id}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  /* Lấy danh sách sản phẩm */
  async function fetchProducts({ page = 1, perPage = 50, filter = '' } = {}) {
    const base = await getApiUrl();
    if (!base) return { items: [], totalItems: 0 };
    const params = new URLSearchParams({ page, perPage });
    if (filter) params.set('filter', filter);
    const res = await fetch(`${base}/api/collections/sanpham/records?${params}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  /* Gửi thông báo đơn hàng về Pi (Flask :5001 hoặc qua API_URL) */
  async function notifyOrder(orderData) {
    const base = await getApiUrl();
    const url = base ? `${base}/notify-order` : 'http://localhost:5001/notify-order';
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(orderData),
      });
      return res.ok;
    } catch (e) {
      console.warn('[API] notifyOrder failed:', e.message);
      return false;
    }
  }

  window.API = { getApiUrl, fetchProduct, fetchProducts, notifyOrder, fileUrl };
})();

