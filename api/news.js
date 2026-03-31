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
    const upstream = await fetch(`${piUrl}/api/news?${searchParams}`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(5000),
    }).catch(() => null);

    if (upstream && upstream.ok) {
      const raw = await upstream.json();
      const items = Array.isArray(raw) ? raw : (raw.items || []);
      return Response.json({ items, totalItems: items.length }, {
        headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
      });
    }
    return Response.json({ items: [], totalItems: 0 }, {
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/news]', e.message);
    return Response.json({ items: [], totalItems: 0 });
  }
}
