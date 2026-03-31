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
    const body = await req.arrayBuffer();
    const contentType = req.headers.get('content-type') || '';
    const upstream = await fetch(`${piUrl}/api/upload`, {
      method: 'POST',
      headers: { 'User-Agent': 'ElectroShop-Vercel', 'Content-Type': contentType },
      body,
      signal: AbortSignal.timeout(30000),
    });
    const data = await upstream.json().catch(() => ({}));
    return Response.json(data, {
      status: upstream.status,
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/upload]', e.message);
    return Response.json({ error: 'upload failed' }, {
      status: 503,
      headers: { 'Access-Control-Allow-Origin': '*' },
    });
  }
}
