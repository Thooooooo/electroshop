/* ============================================================
 * api.js — ElectroShop shared API module v1.2
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

  /* Reset cache (dùng khi cần lấy URL mới) */
  function resetCache() { _cached = null; }

  /* Xây URL file ảnh PocketBase */
  function fileUrl(base, collectionId, recordId, filename, thumb) {
    const url = `${base}/api/files/${collectionId}/${recordId}/${filename}`;
    return thumb ? `${url}?thumb=${thumb}` : url;
  }

  /* ─────────────── SHOP – Sản phẩm ─────────────── */

  async function fetchProduct(id) {
    const base = await getApiUrl();
    if (!base) return null;
    const res = await fetch(`${base}/api/collections/sanpham/records/${id}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function fetchProducts({ page = 1, perPage = 50, filter = '', sort = '-created' } = {}) {
    const base = await getApiUrl();
    if (!base) return { items: [], totalItems: 0 };
    const params = new URLSearchParams({ page, perPage, sort });
    if (filter) params.set('filter', filter);
    const res = await fetch(`${base}/api/collections/sanpham/records?${params}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function updateProduct(id, data, authToken) {
    const base = await getApiUrl();
    if (!base) throw new Error('Không có API URL');
    const res = await fetch(`${base}/api/collections/sanpham/records/${id}`, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json', Authorization: authToken || '' },
      body: JSON.stringify(data),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  /* ─────────────── NEWS – Tin tức ─────────────── */

  async function fetchNews({ page = 1, perPage = 20, filter = '', sort = '-created' } = {}) {
    const base = await getApiUrl();
    if (!base) return { items: [], totalItems: 0 };
    const params = new URLSearchParams({ page, perPage, sort });
    if (filter) params.set('filter', filter);
    try {
      const res = await fetch(`${base}/api/collections/tintuc/records?${params}`);
      if (!res.ok) return { items: [], totalItems: 0, error: `HTTP ${res.status}` };
      return res.json();
    } catch (e) {
      return { items: [], totalItems: 0, error: e.message };
    }
  }

  async function fetchNewsItem(id) {
    const base = await getApiUrl();
    if (!base) return null;
    const res = await fetch(`${base}/api/collections/tintuc/records/${id}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function createNews(data, authToken) {
    const base = await getApiUrl();
    if (!base) throw new Error('Không có API URL');
    const res = await fetch(`${base}/api/collections/tintuc/records`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: authToken || '' },
      body: JSON.stringify(data),
    });
    if (!res.ok) { const e = await res.json(); throw new Error(e.message || `HTTP ${res.status}`); }
    return res.json();
  }

  async function updateNews(id, data, authToken) {
    const base = await getApiUrl();
    if (!base) throw new Error('Không có API URL');
    const res = await fetch(`${base}/api/collections/tintuc/records/${id}`, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json', Authorization: authToken || '' },
      body: JSON.stringify(data),
    });
    if (!res.ok) { const e = await res.json(); throw new Error(e.message || `HTTP ${res.status}`); }
    return res.json();
  }

  async function deleteNews(id, authToken) {
    const base = await getApiUrl();
    if (!base) throw new Error('Không có API URL');
    const res = await fetch(`${base}/api/collections/tintuc/records/${id}`, {
      method: 'DELETE',
      headers: { Authorization: authToken || '' },
    });
    return res.ok;
  }

  /* ─────────────── ORDER – Đơn hàng ─────────────── */

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

  /* ─────────────── AI – Sinh mô tả ─────────────── */

  async function generateDesc(productName, category) {
    try {
      const res = await fetch('http://localhost:5001/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: productName, category }),
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      return data.description || '';
    } catch (e) {
      console.warn('[API] generateDesc failed:', e.message);
      return '';
    }
  }

  window.API = {
    getApiUrl, resetCache, fileUrl,
    /* shop */ fetchProduct, fetchProducts, updateProduct,
    /* news */ fetchNews, fetchNewsItem, createNews, updateNews, deleteNews,
    /* order */ notifyOrder,
    /* ai */ generateDesc,
  };
})();

