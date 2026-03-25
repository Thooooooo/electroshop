// Vercel Edge Function – đọc tunnel URL từ GitHub Contents API (luôn fresh, không cache CDN)
// Pi tự động cập nhật url.txt qua start_shop.sh → web nhận URL mới ngay lập tức
export const config = { runtime: 'edge' };

const GITHUB_API =
  'https://api.github.com/repos/Thooooooo/electroshop-tunnel/contents/url.txt';

export default async function handler() {
  try {
    const res = await fetch(GITHUB_API, {
      headers: { 'User-Agent': 'ElectroShop-Vercel' },
      cache: 'no-store',
    });
    if (!res.ok) throw new Error(`GitHub API: ${res.status}`);
    const data = await res.json();
    // Nội dung file được base64-encode bởi GitHub API
    const apiUrl = atob(data.content.replace(/\n/g, '')).trim();
    return Response.json(
      { apiUrl },
      { headers: { 'Cache-Control': 'no-store' } }
    );
  } catch (e) {
    console.error('[config] fetch failed:', e.message);
    return Response.json({ apiUrl: '' });
  }
}
