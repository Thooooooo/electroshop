// Vercel Edge Function – Proxy /api/products → PocketBase (Pi ẩn hoàn toàn)
// Browser chỉ thấy /api/products — không bao giờ thấy địa chỉ Pi
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
    const { searchParams } = new URL(req.url);

    // Forward query params (page, perPage, filter, sort, expand)
    const target = `${piUrl}/api/collections/sanpham/records?${searchParams}`;
    const upstream = await fetch(target, { headers: { 'Content-Type': 'application/json' } });
    const data = await upstream.json();

    return Response.json(data, {
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
