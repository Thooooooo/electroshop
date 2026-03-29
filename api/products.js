// Vercel Edge Function – Proxy /api/products → Flask v2 API (Pi ẩn hoàn toàn)
// Browser chỉ thấy /api/products — không bao giờ thấy địa chỉ Pi
export const config = { runtime: 'edge' };

// URL tunnel Cloudflare → Flask v2 port 8888 trên Pi
const PI_URL = process.env.API_URL || 'https://pros-cases-postposted-accompanying.trycloudflare.com';

// Chuẩn hoá Flask API response → format {items, totalItems} để frontend dùng chung
function normalizeFlaskProducts(raw, piUrl) {
  const arr = Array.isArray(raw) ? raw : (raw.products || []);
  const items = arr.map(p => ({
    id:          p.id,
    ten:         p.name  || p.ten  || '',
    mota:        p.description || p.mota || '',
    loai:        p.category || p.loai || '',
    gia:         typeof p.gia === 'number' ? p.gia
                   : (parseFloat(String(p.price || '0').replace(/[^\d.]/g, '')) || 0),
    icon:        p.icon || '📦',
    // Ảnh Flask: /static/uploads/<filename>; ảnh PocketBase: dùng fileUrl()
    anh_url:     p.image ? `${piUrl}/static/uploads/${p.image}` : '',
    _source:     'flask',
  }));
  return { items, totalItems: items.length };
}

export default async function handler(req) {
  try {
    const piUrl = PI_URL;

    const upstream = await fetch(`${piUrl}/api/products`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(8000),
    });
    if (!upstream.ok) throw new Error(`upstream ${upstream.status}`);

    const raw = await upstream.json();
    const normalized = normalizeFlaskProducts(raw, piUrl);

    return Response.json(normalized, {
      headers: {
        'Cache-Control': 'no-store',
        'Access-Control-Allow-Origin': '*',
      },
    });
  } catch (e) {
    console.error('[proxy/products]', e.message);
    return Response.json({ items: [], totalItems: 0, error: 'unavailable' }, { status: 503 });
  }
}
