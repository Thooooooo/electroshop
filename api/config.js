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

export default async function handler() {
  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    const health = await fetch(`${piUrl}/health`, {
      signal: AbortSignal.timeout(5000),
    }).catch(() => null);
    return Response.json(
      { connected: health?.ok ?? false, ts: Date.now() },
      { headers: { 'Cache-Control': 'no-store' } }
    );
  } catch (e) {
    return Response.json({ connected: false, ts: Date.now() });
  }
}
