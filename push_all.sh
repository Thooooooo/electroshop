#!/bin/bash
# Push to both GitHub and Vercel at the same time

echo "📤 Pushing to GitHub..."
git push origin main || exit 1

echo "📤 Pushing to Vercel..."
git push vercel-origin main || exit 1

echo "✅ Pushed to both repositories!"
echo ""
echo "📍 Check Vercel deployment:"
echo "   https://vercel.com/dashboard"
