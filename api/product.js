// Vercel Edge Function – Proxy /api/product?id=X → Flask v2 single product
export const config = { runtime: 'edge' };

// URL tunnel Cloudflare → Flask v2 port 8888 trên Pi
const PI_URL = process.env.API_URL || 'https://pros-cases-postposted-accompanying.trycloudflare.com';

export default async function handler(req) {
  try {
    const { searchParams, pathname } = new URL(req.url);

    // Extract id from /api/product?id=xxx or /api/product/xxx
    const id = searchParams.get('id') || pathname.split('/').pop();
    if (!id) return Response.json({ error: 'id required' }, { status: 400 });

    // Flask v2 không có endpoint /api/products/:id — lấy all rồi filter
    const upstream = await fetch(`${PI_URL}/api/products`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(8000),
    });
    if (!upstream.ok) throw new Error(`upstream ${upstream.status}`);

    const all = await upstream.json();
    const arr = Array.isArray(all) ? all : (all.products || []);
    // Match theo id (số nguyên) hoặc slug/name
    const item = arr.find(p => String(p.id) === String(id)) || null;
    if (!item) return Response.json({ error: 'not found' }, { status: 404 });

    // Chuẩn hoá về format chung
    const normalized = {
      id:          item.id,
      ten:         item.name  || item.ten  || '',
      name:        item.name  || item.ten  || '',
      mota:        item.description || item.mota || '',
      mo_ta:       item.description || item.mota || '',
      loai:        item.category || item.loai || '',
      gia:         parseFloat(String(item.price || '0').replace(/[^\d.]/g, '')) || 0,
      price:       item.price || '0',
      icon:        item.icon || '📦',
      anh_url:     item.image ? `${PI_URL}/static/uploads/${item.image}` : '',
      stock:       item.stock ?? 0,
      _source:     'flask',
    };

    return Response.json(normalized, {
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/product]', e.message);
    return Response.json({ error: 'unavailable' }, { status: 503 });
  }
}
