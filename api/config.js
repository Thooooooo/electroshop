// Vercel Edge Function – đọc tunnel URL trực tiếp từ GitHub public repo
// Pi tự động cập nhật url.txt qua GitHub API → web nhận URL mới ngay, không cần redeploy
export const config = { runtime: 'edge' };

const TUNNEL_URL_RAW =
  'https://raw.githubusercontent.com/Thooooooo/electroshop-tunnel/main/url.txt';

export default async function handler() {
  try {
    const res = await fetch(TUNNEL_URL_RAW, { cache: 'no-store' });
    const apiUrl = (await res.text()).trim();
    return Response.json(
      { apiUrl },
      { headers: { 'Cache-Control': 'no-store' } }
    );
  } catch (e) {
    return Response.json({ apiUrl: '' });
  }
}
