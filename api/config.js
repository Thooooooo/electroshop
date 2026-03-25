// Vercel Edge Function – Kiểm tra kết nối Pi (KHÔNG trả URL về browser)
// URL Pi chỉ tồn tại trong Vercel env vars — không bao giờ expose ra client
export const config = { runtime: 'edge' };

async function getPiUrl() {
  const GITHUB_API = 'https://api.github.com/repos/Thooooooo/electroshop-tunnel/contents/url.txt';
  const res = await fetch(GITHUB_API, {
    headers: { 'User-Agent': 'ElectroShop-Health' },
    cache: 'no-store',
  });
  if (!res.ok) throw new Error(`GitHub ${res.status}`);
  const data = await res.json();
  return atob(data.content.replace(/\n/g, '')).trim();
}

export default async function handler() {
  try {
    const piUrl = process.env.API_URL || await getPiUrl();
    // Kiểm tra Pi còn sống không (không trả URL ra ngoài)
    const health = await fetch(`${piUrl}/api/health`, {
      signal: AbortSignal.timeout(5000),
    }).catch(() => null);

    const connected = health?.ok ?? false;
    return Response.json(
      { connected, ts: Date.now() },
      { headers: { 'Cache-Control': 'no-store' } }
    );
  } catch (e) {
    return Response.json({ connected: false, ts: Date.now() });
  }
}

