// Vercel Edge Function – Kiểm tra kết nối Pi (KHÔNG trả URL về browser)
// URL Pi chỉ tồn tại trong Vercel env vars — không bao giờ expose ra client
export const config = { runtime: 'edge' };

// URL tunnel Cloudflare → Flask v2 port 8888 trên Pi
const PI_URL = process.env.API_URL || 'https://stomach-skating-days-therapy.trycloudflare.com';

export default async function handler() {
  try {
    const health = await fetch(`${PI_URL}/api/health`, {
      signal: AbortSignal.timeout(5000),
    }).catch(() => null);

    // Flask /api/health có thể không có — thử /api/products nếu không
    const connected = health?.ok ?? false;
    return Response.json(
      { connected, ts: Date.now() },
      { headers: { 'Cache-Control': 'no-store' } }
    );
  } catch (e) {
    return Response.json({ connected: false, ts: Date.now() });
  }
}

