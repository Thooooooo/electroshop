/* ============================================================
 * api.js — ElectroShop shared API module v1.3
 * ┌─────────────────────────────────────────────────────────┐
 * │ SECURITY:                                               │
 * │  • Khi chạy trên Vercel (remote):                       │
 * │    → dùng /api/products, /api/news, /api/order          │
 * │    → Vercel proxy ẩn hoàn toàn địa chỉ Pi 5            │
 * │    → F12 không bao giờ thấy URL trycloudflare.com       │
 * │  • Khi chạy trên localhost/Pi (admin):                  │
 * │    → gọi thẳng PocketBase :8090 để tránh độ trễ         │
 * └─────────────────────────────────────────────────────────┘
 * ============================================================ */
(function () {
  'use strict';

  /* Phát hiện môi trường: local Pi hay Vercel */
  const IS_LOCAL = (
    location.hostname === 'localhost' ||
    location.hostname === '127.0.0.1' ||
    /^192\.168\.|^10\.|^172\.(1[6-9]|2\d|3[01])\./.test(location.hostname)
  );

  // Dùng hostname thực của Pi thay vì 'localhost' để iPad/điện thoại trên LAN
  // cũng kết nối được — 'localhost:8090' chỉ đúng khi chính Pi tự duyệt web.
  const _host      = (location.hostname === 'localhost' || location.hostname === '127.0.0.1')
                       ? 'localhost'
                       : location.hostname;
  const LOCAL_PB   = `http://${_host}:8090`;   // PocketBase — đúng với mọi thiết bị trên LAN
  const LOCAL_FLASK = `http://${_host}:5001`;  // Flask AI

  /* ─────────────── Xây URL file ảnh ─────────────── */
  function fileUrl(base, collectionId, recordId, filename, thumb) {
    const url = `${base}/api/files/${collectionId}/${recordId}/${filename}`;
    return thumb ? `${url}?thumb=${thumb}` : url;
  }

  /* Khi trên Vercel: trả về proxy URL prefix; khi local: trả PB URL */
  async function getApiUrl() {
    return IS_LOCAL ? LOCAL_PB : '';
  }

  /* ─────────────── SHOP – Sản phẩm ─────────────── */

  async function fetchProduct(id) {
    if (IS_LOCAL) {
      const res = await fetch(`${LOCAL_PB}/api/collections/sanpham/records/${id}`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json();
    }
    // Vercel proxy — Pi URL ẩn hoàn toàn
    const res = await fetch(`/api/product?id=${encodeURIComponent(id)}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function fetchProducts({ page = 1, perPage = 50, filter = '', sort = '-created' } = {}) {
    const params = new URLSearchParams({ page, perPage, sort });
    if (filter) params.set('filter', filter);

    if (IS_LOCAL) {
      const res = await fetch(`${LOCAL_PB}/api/collections/sanpham/records?${params}`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json();
    }
    // Vercel proxy
    const res = await fetch(`/api/products?${params}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function updateProduct(id, data, authToken) {
    const base = IS_LOCAL ? LOCAL_PB : await _resolveRemoteBase();
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
    const params = new URLSearchParams({ page, perPage, sort });
    if (filter) params.set('filter', filter);

    if (IS_LOCAL) {
      try {
        const res = await fetch(`${LOCAL_PB}/api/collections/tintuc/records?${params}`);
        if (!res.ok) return { items: [], totalItems: 0 };
        return res.json();
      } catch (e) { return { items: [], totalItems: 0 }; }
    }
    try {
      const res = await fetch(`/api/news?${params}`);
      if (!res.ok) return { items: [], totalItems: 0 };
      return res.json();
    } catch (e) { return { items: [], totalItems: 0 }; }
  }

  async function fetchNewsItem(id) {
    if (IS_LOCAL) {
      const res = await fetch(`${LOCAL_PB}/api/collections/tintuc/records/${id}`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res.json();
    }
    const res = await fetch(`/api/news?id=${encodeURIComponent(id)}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  async function createNews(data, authToken) {
    const base = IS_LOCAL ? LOCAL_PB : await _resolveRemoteBase();
    const res = await fetch(`${base}/api/collections/tintuc/records`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: authToken || '' },
      body: JSON.stringify(data),
    });
    if (!res.ok) { const e = await res.json(); throw new Error(e.message || `HTTP ${res.status}`); }
    return res.json();
  }

  async function updateNews(id, data, authToken) {
    const base = IS_LOCAL ? LOCAL_PB : await _resolveRemoteBase();
    const res = await fetch(`${base}/api/collections/tintuc/records/${id}`, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json', Authorization: authToken || '' },
      body: JSON.stringify(data),
    });
    if (!res.ok) { const e = await res.json(); throw new Error(e.message || `HTTP ${res.status}`); }
    return res.json();
  }

  async function deleteNews(id, authToken) {
    const base = IS_LOCAL ? LOCAL_PB : await _resolveRemoteBase();
    const res = await fetch(`${base}/api/collections/tintuc/records/${id}`, {
      method: 'DELETE',
      headers: { Authorization: authToken || '' },
    });
    return res.ok;
  }

  /* ─────────────── ORDER – Đơn hàng ─────────────── */

  async function notifyOrder(orderData) {
    if (IS_LOCAL) {
      try {
        const res = await fetch(`${LOCAL_FLASK}/notify-order`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(orderData),
        });
        return res.ok;
      } catch (e) { return false; }
    }
    // Vercel proxy — browser không thấy URL Pi
    try {
      const res = await fetch('/api/order', {
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

  /* ─────────────── AI – Sinh mô tả (local only) ─────────────── */

  async function generateDesc(productName, category) {
    try {
      const res = await fetch(`${LOCAL_FLASK}/generate`, {
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

  /* ─────────────── Internal helpers ─────────────── */

  /* Dùng cho admin write operations khi không local (cần Pi URL thật) */
  let _piUrlCache = null;
  async function _resolveRemoteBase() {
    if (_piUrlCache) return _piUrlCache;
    try {
      const GITHUB_API = 'https://api.github.com/repos/Thooooooo/electroshop-tunnel/contents/url.txt';
      const res = await fetch(GITHUB_API, { headers: { 'User-Agent': 'ES-Admin' }, cache: 'no-store' });
      const data = await res.json();
      _piUrlCache = atob(data.content.replace(/\n/g, '')).trim();
    } catch (e) {
      _piUrlCache = '';
    }
    return _piUrlCache;
  }

  /* Reset caches */
  function resetCache() { _piUrlCache = null; }

  window.API = {
    getApiUrl, resetCache, fileUrl, isLocal: IS_LOCAL,
    /* shop */  fetchProduct, fetchProducts, updateProduct,
    /* news */  fetchNews, fetchNewsItem, createNews, updateNews, deleteNews,
    /* order */ notifyOrder,
    /* ai */    generateDesc,
  };
})();

