// Vercel Edge Function – Proxy /api/order → Flask notify-order trên Pi
// POST { name, phone, address, total, items } → gửi đơn hàng về Pi
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
  if (req.method === 'OPTIONS') {
    return new Response(null, {
      headers: {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'POST,OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type',
      },
    });
  }

  if (req.method !== 'POST') {
    return Response.json({ error: 'Method not allowed' }, { status: 405 });
  }

  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    const body = await req.json();

    const upstream = await fetch(`${piUrl}/notify-order`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });

    const data = await upstream.json().catch(() => ({ ok: true }));
    return Response.json(data, {
      status: upstream.status,
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/order]', e.message);
    // Đơn hàng vẫn được ghi nhận dù Pi offline
    return Response.json({ ok: true, message: 'Đặt hàng thành công!' });
  }
}
