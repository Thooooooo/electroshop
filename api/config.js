// Vercel Edge Function – trả về API_URL từ environment variable
// Đặt biến môi trường API_URL trong Vercel Dashboard (Settings → Environment Variables)
export const config = { runtime: 'edge' };

export default function handler() {
  return Response.json(
    { apiUrl: process.env.API_URL || '' },
    { headers: { 'Cache-Control': 'public, s-maxage=60' } }
  );
}
