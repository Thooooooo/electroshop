// Vercel Edge Function – Proxy /api/news → Flask v2 (news not yet implemented → empty)
export const config = { runtime: 'edge' };

// URL tunnel Cloudflare → Flask v2 port 8888 trên Pi
const PI_URL = process.env.API_URL || 'https://stomach-skating-days-therapy.trycloudflare.com';

export default async function handler(req) {
  try {
    const { searchParams } = new URL(req.url);

    // Thử gọi Flask /api/news nếu có, fallback về empty
    const upstream = await fetch(`${PI_URL}/api/news?${searchParams}`, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      signal: AbortSignal.timeout(5000),
    }).catch(() => null);

    if (upstream && upstream.ok) {
      const data = await upstream.json();
      return Response.json(data, {
        headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
      });
    }

    // Flask chưa có /api/news → trả mảng rỗng, không báo lỗi
    return Response.json({ items: [], totalItems: 0 }, {
      headers: { 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' },
    });
  } catch (e) {
    console.error('[proxy/news]', e.message);
    return Response.json({ items: [], totalItems: 0 });
  }
}

