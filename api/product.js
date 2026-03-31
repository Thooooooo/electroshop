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

export default async function handler(req) {
  try {
    const { searchParams, pathname } = new URL(req.url);
    const id = searchParams.get('id') || pathname.split('/').pop();
    if (!id) return Response.json({ error: 'id required' }, { status: 400 });

    const piUrl = process.env.API_URL || await getPiUrl();
    const upstream = await fetch(`${piUrl}/api/products`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(8000),
    });
    if (!upstream.ok) throw new Error(`upstream ${upstream.status}`);

    const all = await upstream.json();
    const arr = Array.isArray(all) ? all : (all.products || []);
    const item = arr.find(p => String(p.id) === String(id)) || null;
    if (!item) return Response.json({ error: 'not found' }, { status: 404 });

    return Response.json({
      id:      item.id,
      ten:     item.name || item.ten || '',
      name:    item.name || item.ten || '',
      mota:    item.description || item.mota || '',
      mo_ta:   item.description || item.mota || '',
      loai:    item.category || item.loai || '',
      gia:     parseFloat(String(item.price || '0').replace(/[^\d.]/g, '')) || 0,
      price:   item.price || '0',
      icon:    item.icon || '📦',
      anh_url: item.image ? `${piUrl}/static/uploads/${item.image}` : '',
      stock:   item.stock ?? 0,
      _source: 'flask',
    }, {
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/product]', e.message);
    return Response.json({ error: 'unavailable' }, { status: 503 });
  }
}
