// Vercel Edge Function – Proxy /api/product/[id] → PocketBase single record
export const config = { runtime: 'edge' };

async function getPiUrl() {
  const GITHUB_API = 'https://api.github.com/repos/Thooooooo/electroshop-tunnel/contents/url.txt';
  const res = await fetch(GITHUB_API, {
    headers: { 'User-Agent': 'ElectroShop-Proxy' },
    cache: 'no-store',
  });
  if (!res.ok) throw new Error(`GitHub ${res.status}`);
  const data = await res.json();
  return atob(data.content.replace(/\n/g, '')).trim();
}

export default async function handler(req) {
  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    const { searchParams, pathname } = new URL(req.url);

    // Extract id from /api/product?id=xxx or /api/product/xxx
    const id = searchParams.get('id') || pathname.split('/').pop();
    if (!id) return Response.json({ error: 'id required' }, { status: 400 });

    const upstream = await fetch(
      `${piUrl}/api/collections/sanpham/records/${id}`,
      { headers: { 'Content-Type': 'application/json' } }
    );
    const data = await upstream.json();

    return Response.json(data, {
      status: upstream.status,
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/product]', e.message);
    return Response.json({ error: 'unavailable' }, { status: 503 });
  }
}
