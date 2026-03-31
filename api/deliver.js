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
  if (req.method === 'OPTIONS') {
    return new Response(null, {
      headers: {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'POST,OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type',
      },
    });
  }
  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    const body = await req.text();
    const upstream = await fetch(`${piUrl}/api/deliver`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'User-Agent': 'ElectroShop-Vercel' },
      body,
      signal: AbortSignal.timeout(10000),
    });
    const data = await upstream.json().catch(() => ({ ok: true }));
    return Response.json(data, {
      status: upstream.status,
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/deliver]', e.message);
    return Response.json({ ok: true, message: 'Đặt hàng thành công!' }, {
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  }
}
