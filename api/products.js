export const config = { runtime: 'edge' };

async function getPiUrl() {
  const res = await fetch('https://api.github.com/repos/Thooooooo/electroshop-tunnel/contents/url.txt', {
    headers: { 'User-Agent': 'ElectroShop-Proxy' },
    cache: 'no-store',
  });
  if (!res.ok) throw new Error(`GitHub ${res.status}`);
  const data = await res.json();
  return atob(data.content.replace(/\n/g, '')).trim();
}

function normalizeFlaskProducts(raw, piUrl) {
  const arr = Array.isArray(raw) ? raw : (raw.products || []);
  const items = arr.map(p => ({
    id:      p.id,
    ten:     p.name || p.ten || '',
    mota:    p.description || p.mota || '',
    loai:    p.category || p.loai || '',
    gia:     parseFloat(String(p.price || '0').replace(/[^\d.]/g, '')) || 0,
    icon:    p.icon || '📦',
    anh_url: p.image ? `${piUrl}/static/uploads/${p.image}` : '',
    _source: 'flask',
  }));
  return { items, totalItems: items.length };
}

export default async function handler(req) {
  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    const upstream = await fetch(`${piUrl}/api/products`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(8000),
    });
    if (!upstream.ok) throw new Error(`upstream ${upstream.status}`);
    const raw = await upstream.json();
    return Response.json(normalizeFlaskProducts(raw, piUrl), {
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/products]', e.message);
    return Response.json({ items: [], totalItems: 0, error: 'unavailable' }, { status: 503 });
  }
}
