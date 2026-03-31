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
    const { searchParams } = new URL(req.url);
    const piUrl = process.env.API_URL || await getPiUrl();
    const upstream = await fetch(`${piUrl}/api/order-status?${searchParams}`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(10000),
    });
    if (!upstream.ok) throw new Error(`upstream ${upstream.status}`);
    const data = await upstream.json();
    return Response.json(data, {
      headers: { 'Access-Control-Allow-Origin': '*', 'Cache-Control': 'no-store' },
    });
  } catch (e) {
    console.error('[proxy/order-status]', e.message);
    return Response.json({ success: false, paid: false, error: 'unavailable' }, {
      status: 503,
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  }
}
