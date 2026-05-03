from flask import Flask, request, redirect, render_template_string, jsonify, abort, send_from_directory
from flask_cors import CORS
from werkzeug.middleware.proxy_fix import ProxyFix
from werkzeug.utils import secure_filename
import sqlite3, os, uuid, time, threading
from urllib.parse import quote
import requests
import json

app = Flask(__name__)
CORS(app)
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1)

# ── PocketBase Configuration ──────────────────────────────────────────────────
POCKETBASE_URL = 'http://localhost:8090'
POCKETBASE_SANPHAM_COLLECTION = 'sanpham'

@app.after_request
def add_cors(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Methods'] = 'GET, POST, PUT, DELETE, OPTIONS'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type, Authorization'
    return response

# ── PocketBase Helpers ────────────────────────────────────────────────────────

def fetch_from_pocketbase():
    """Fetch all products from PocketBase sanpham collection"""
    try:
        url = f'{POCKETBASE_URL}/api/collections/{POCKETBASE_SANPHAM_COLLECTION}/records?perPage=200'
        response = requests.get(url, timeout=5)
        response.raise_for_status()
        data = response.json()
        items = data.get('items', [])
        # Normalize PocketBase fields to SQLite field names
        normalized = []
        for item in items:
            # Handle image/anh field safely
            image_name = ''
            anh_list = item.get('anh', [])
            if isinstance(anh_list, list) and len(anh_list) > 0:
                image_name = anh_list[0]
            elif not anh_list and item.get('image'):
                image_name = item.get('image', '')
            
            normalized.append({
                'id': item.get('id', ''),
                'name': item.get('ten', item.get('name', '')),
                'ten': item.get('ten', item.get('name', '')),
                'price': str(item.get('gia', item.get('price', 0))),
                'gia': item.get('gia', item.get('price', 0)),
                'category': item.get('loai', item.get('category', '')),
                'loai': item.get('loai', item.get('category', '')),
                'description': item.get('mo_ta', item.get('description', '')),
                'mo_ta': item.get('mo_ta', item.get('description', '')),
                'stock': item.get('stock', 0),
                'icon': item.get('icon', '📦'),
                'image': image_name,
                'created_at': item.get('created', item.get('created_at', '')),
                'collectionId': item.get('collectionId', ''),
            })
        return normalized
    except Exception as e:
        print(f'[PocketBase] Error fetching products: {e}')
        return []

def add_to_pocketbase(data):
    """Add product to PocketBase"""
    def parse_price(price_input):
        """Convert price string to integer (handles VN format: 1.999.999 or 99,99)"""
        try:
            s = str(price_input).strip()
            # Remove currency symbols
            s = s.replace('₫', '').replace('VND', '').strip()
            
            # Detect format: if has comma, it's decimal; dots are thousands separators
            if ',' in s:
                # Format: "1.234,56" → remove dots, replace comma with dot
                s = s.replace('.', '').replace(',', '.')
                return int(float(s))
            else:
                # Format: "1234567" or "1.234.567" → remove dots (thousands separator)
                s = s.replace('.', '')
                return int(float(s))
        except:
            return 0
    
    try:
        url = f'{POCKETBASE_URL}/api/collections/{POCKETBASE_SANPHAM_COLLECTION}/records'
        payload = {
            'ten': data.get('name', ''),
            'gia': parse_price(data.get('price', 0)),
            'loai': data.get('category', ''),
            'mo_ta': data.get('description', ''),
            'stock': int(data.get('stock', 0)),
            'icon': data.get('icon', '📦'),
        }
        response = requests.post(url, json=payload, timeout=5)
        response.raise_for_status()
        return response.json()
    except Exception as e:
        print(f'[PocketBase] Error adding product: {e}')
        return None

# ── Vercel / local path detection ────────────────────────────────────────────
IS_VERCEL = bool(os.environ.get('VERCEL') or os.environ.get('VERCEL_ENV'))

_BASE = os.path.dirname(os.path.abspath(__file__))
DB         = '/tmp/v2.db'        if IS_VERCEL else os.path.join(_BASE, 'v2.db')
UPLOAD_DIR = '/tmp/uploads'      if IS_VERCEL else os.path.join(_BASE, 'static', 'uploads')
IMAGES_DIR_DEFAULT = '/tmp/images' if IS_VERCEL else os.path.join(_BASE, 'static', 'images')

ALLOWED_EXT = {'jpg', 'jpeg', 'png', 'gif', 'webp'}

os.makedirs(UPLOAD_DIR, exist_ok=True)

# ── Helpers ───────────────────────────────────────────────────────────────────

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXT

def save_upload(file_obj):
    if not file_obj or file_obj.filename == '':
        return ''
    if not allowed_file(file_obj.filename):
        return ''
    ext = file_obj.filename.rsplit('.', 1)[1].lower()
    fname = uuid.uuid4().hex + '.' + ext
    file_obj.save(os.path.join(UPLOAD_DIR, fname))
    return fname

def delete_image(img):
    if img:
        try:
            os.remove(os.path.join(UPLOAD_DIR, img))
        except Exception:
            pass

# ── Database ──────────────────────────────────────────────────────────────────

def get_db():
    c = sqlite3.connect(DB)
    c.row_factory = sqlite3.Row
    return c

def normalize_product(row):
    """Convert a SQLite Row (or dict) to a normalized product dict with both
    Vietnamese and English field aliases so the frontend never breaks."""
    d = dict(row)
    return {
        'id':          d.get('id'),
        'name':        d.get('name', ''),
        'ten':         d.get('name', ''),
        'price':       str(d.get('price', 0)),
        'gia':         d.get('price', 0),
        'category':    d.get('category', ''),
        'loai':        d.get('category', ''),
        'description': d.get('description', ''),
        'mo_ta':       d.get('description', ''),
        'stock':       d.get('stock', 0),
        'icon':        d.get('icon', '📦'),
        'image':       d.get('image', ''),
        'anh_url':     d.get('image', ''),
        'created_at':  str(d.get('created_at', '')),
    }


def init_db():
    with get_db() as conn:
        conn.execute(
            "CREATE TABLE IF NOT EXISTS products ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT NOT NULL, price TEXT NOT NULL,"
            "category TEXT DEFAULT '', description TEXT DEFAULT '',"
            "stock INTEGER DEFAULT 0, icon TEXT DEFAULT '📦',"
            "image TEXT DEFAULT '', created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        )
        conn.execute(
            "CREATE TABLE IF NOT EXISTS news ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL, content TEXT NOT NULL,"
            "summary TEXT DEFAULT '', cover TEXT DEFAULT '',"
            "author TEXT DEFAULT 'Admin',"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        )
        conn.commit()
        for col in [
            "ALTER TABLE products ADD COLUMN image TEXT DEFAULT ''",
            "ALTER TABLE news ADD COLUMN summary TEXT DEFAULT ''",
        ]:
            try:
                conn.execute(col)
                conn.commit()
            except Exception:
                pass
        if conn.execute("SELECT COUNT(*) FROM products").fetchone()[0] == 0:
            conn.executemany(
                "INSERT INTO products(name,price,category,description,stock,icon) VALUES(?,?,?,?,?,?)",
                [
                    ("iPhone 15 Pro",  "27.990.000₫", "Điện thoại",    "Chip A17 Pro, camera 48MP",       10, "📱"),
                    ("MacBook Air M3", "28.990.000₫", "Laptop",        "M3 chip, 8GB RAM, 18h pin",        5, "💻"),
                    ("AirPods Pro 2",  "6.490.000₫",  "Tai nghe",      "ANC, Transparency, USB-C",        20, "🎧"),
                    ("Apple Watch S9", "10.990.000₫", "Đồng hồ",       "S9 chip, màn hình Always-On",      8, "⌚"),
                    ("iPad Pro M4",    "24.990.000₫", "Máy tính bảng", "OLED 11 inch, WiFi 6E, M4",        6, "🔋"),
                ]
            )
            conn.commit()
        if conn.execute("SELECT COUNT(*) FROM news").fetchone()[0] == 0:
            conn.executemany(
                "INSERT INTO news(title,content,summary,cover,author) VALUES(?,?,?,?,?)",
                [
                    ("Ra mắt iPhone 16",
                     "Apple vừa ra mắt iPhone 16 với nhiều cải tiến vượt bậc so với thế hệ trước.",
                     "Tóm tắt nhanh", "", "Admin"),
                    ("Review MacBook M3",
                     "Hiệu năng MacBook M3 vượt trội, pin bền, màn hình sắc nét tuyệt vời.",
                     "Đánh giá chi tiết", "", "Admin"),
                    ("Top 5 Tai Nghe 2025",
                     "Những mẫu tai nghe đáng mua nhất năm 2025.",
                     "Tổng hợp hay nhất", "", "Admin"),
                ]
            )
            conn.commit()

# ── Shared CSS ────────────────────────────────────────────────────────────────

SHARED_CSS = (
    ":root{"
    "--bg:#f0f4f8;--surface:#fff;--text:#1a202c;--muted:#718096;"
    "--border:#e2e8f0;--radius:1rem;--shadow:0 4px 20px rgba(99,102,241,.12);"
    "--p:#6366f1;--p2:#8b5cf6;"
    "}"
    "[data-theme=dark]{"
    "--bg:#0f172a;--surface:#1e293b;--text:#f1f5f9;--muted:#94a3b8;"
    "--border:#334155;--shadow:0 4px 20px rgba(0,0,0,.4);"
    "}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{background:var(--bg);color:var(--text);font-family:'Segoe UI',sans-serif;min-height:100vh}"
    "a{text-decoration:none}"
    # Header
    ".hdr{position:sticky;top:0;z-index:100;"
    "background:linear-gradient(135deg,#6366f1,#8b5cf6);"
    "height:58px;display:flex;align-items:center;gap:12px;padding:0 20px}"
    ".logo{color:#fff;font-size:1.3rem;font-weight:800;white-space:nowrap}"
    ".logo:hover{color:#fff}"
    ".search-input{flex:1;max-width:440px;padding:7px 18px;border-radius:999px;"
    "border:2px solid transparent;background:rgba(255,255,255,.2);color:#fff;"
    "font-size:.9rem;outline:none;transition:.2s}"
    ".search-input::placeholder{color:rgba(255,255,255,.7)}"
    ".search-input:focus{box-shadow:0 0 0 3px rgba(99,102,241,.5);border-color:#a78bfa;"
    "background:rgba(255,255,255,.25)}"
    ".hdr-nav{display:flex;gap:4px}"
    ".hdr-nav a{color:rgba(255,255,255,.8);padding:4px 12px;border-radius:6px;"
    "font-size:.88rem;font-weight:500;transition:.15s}"
    ".hdr-nav a.active{background:rgba(255,255,255,.25);color:#fff}"
    ".hdr-nav a:hover:not(.active){background:rgba(255,255,255,.15);color:#fff}"
    ".hdr-right{display:flex;gap:8px;align-items:center;margin-left:auto}"
    ".btn-hdr{padding:5px 14px;border-radius:7px;border:none;font-size:.83rem;"
    "font-weight:600;cursor:pointer;transition:.15s}"
    ".btn-add{background:rgba(255,255,255,.22);color:#fff}"
    ".btn-add:hover{background:rgba(255,255,255,.38)}"
    ".btn-theme{background:rgba(255,255,255,.15);color:#fff;width:34px;height:34px;"
    "border-radius:50%;padding:0;display:flex;align-items:center;justify-content:center}"
    ".btn-theme:hover{background:rgba(255,255,255,.3)}"
    # Layout
    ".container{max-width:1200px;margin:0 auto;padding:1.5rem}"
    ".page-title{font-size:1.6rem;font-weight:800;margin-bottom:.35rem;margin-top:1.5rem}"
    ".section-sub{color:var(--muted);margin-bottom:1.4rem;font-size:.9rem}"
    # Product grid
    ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(255px,1fr));gap:1.2rem}"
    "a.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);"
    "overflow:hidden;transition:.2s;display:flex;flex-direction:column;"
    "text-decoration:none;color:var(--text)}"
    "a.card:hover{transform:translateY(-3px);box-shadow:0 8px 24px rgba(99,102,241,.15)}"
    ".card-img{height:150px;overflow:hidden;"
    "background:linear-gradient(135deg,#eef2ff,#f5f3ff);"
    "display:flex;align-items:center;justify-content:center}"
    "[data-theme=dark] .card-img{background:linear-gradient(135deg,#1e2a4a,#2d1b69)}"
    ".card-img img{width:100%;height:150px;object-fit:cover}"
    ".card-icon{font-size:3rem}"
    ".card-body{padding:1rem;flex:1;display:flex;flex-direction:column;gap:.3rem}"
    ".card-name{font-weight:700;font-size:1rem}"
    ".card-price{color:var(--p);font-weight:700;font-size:1.05rem}"
    ".card-cat{font-size:.8rem;color:var(--muted)}"
    ".card-stock{font-size:.8rem;color:var(--muted)}"
    # News cards
    ".news-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:1.2rem}"
    "a.news-card{background:var(--surface);border:1px solid var(--border);"
    "border-radius:var(--radius);overflow:hidden;transition:.2s;"
    "display:block;text-decoration:none;color:var(--text)}"
    "a.news-card:hover{transform:translateY(-3px);box-shadow:var(--shadow)}"
    ".news-cover{height:140px;overflow:hidden;"
    "background:linear-gradient(135deg,#ede9fe,#ddd6fe);"
    "display:flex;align-items:center;justify-content:center;font-size:2.5rem}"
    ".news-cover img{width:100%;height:140px;object-fit:cover}"
    ".news-body{padding:1rem}"
    ".news-title{font-weight:700;font-size:.97rem;margin-bottom:6px;line-height:1.4}"
    ".news-summary{font-size:.83rem;color:var(--muted);line-height:1.5}"
    ".news-meta{font-size:.75rem;color:var(--muted);margin-top:8px}"
    # Detail
    ".detail-card{background:var(--surface);border:1px solid var(--border);"
    "border-radius:var(--radius);padding:2rem;max-width:680px;margin:2rem auto}"
    ".detail-img{width:100%;max-height:320px;object-fit:cover;"
    "border-radius:.75rem;margin-bottom:1rem}"
    ".detail-icon{font-size:5rem;display:block;text-align:center;margin-bottom:1rem}"
    ".detail-name{font-size:1.8rem;font-weight:800;margin-bottom:.5rem}"
    ".detail-price{font-size:1.4rem;color:var(--p);font-weight:700;margin-bottom:1rem}"
    ".badge{display:inline-block;background:#e0e7ff;color:#4338ca;border-radius:2rem;"
    "padding:.2rem .75rem;font-size:.8rem;font-weight:600}"
    "[data-theme=dark] .badge{background:#312e81;color:#a5b4fc}"
    # Article
    ".article-title{font-size:1.8rem;font-weight:800;line-height:1.3;margin-bottom:.75rem}"
    ".article-meta{font-size:.85rem;color:var(--muted);margin-bottom:1.5rem}"
    ".article-cover{width:100%;max-height:360px;object-fit:cover;"
    "border-radius:.75rem;margin-bottom:1.5rem}"
    ".article-cover-ph{width:100%;height:220px;"
    "background:linear-gradient(135deg,#ede9fe,#ddd6fe);"
    "border-radius:.75rem;display:flex;align-items:center;"
    "justify-content:center;font-size:4rem;margin-bottom:1.5rem}"
    ".article-content{line-height:1.8;white-space:pre-wrap;font-size:.97rem}"
    # Forms
    ".form-card{background:var(--surface);border:1px solid var(--border);"
    "border-radius:var(--radius);padding:2rem;max-width:540px;margin:2rem auto}"
    ".form-card h2{margin-bottom:1.5rem;font-size:1.4rem}"
    ".form-group{margin-bottom:1rem}"
    "label{display:block;font-size:.85rem;font-weight:600;color:var(--muted);margin-bottom:.3rem}"
    "input[type=text],input[type=number],textarea,select,input[type=file]{"
    "width:100%;padding:.6rem .9rem;border:1px solid var(--border);border-radius:.5rem;"
    "font-size:.95rem;background:var(--bg);color:var(--text);outline:none}"
    "input:focus,textarea:focus,select:focus{border-color:var(--p)}"
    "textarea{resize:vertical;min-height:80px}"
    ".actions{display:flex;gap:.75rem;margin-top:1.5rem;flex-wrap:wrap}"
    ".btn{display:inline-block;padding:.45rem 1.1rem;border-radius:.5rem;font-size:.9rem;"
    "font-weight:600;cursor:pointer;border:none;text-decoration:none;transition:.15s}"
    ".btn-primary{background:linear-gradient(135deg,var(--p),var(--p2));color:#fff}"
    ".btn-primary:hover{opacity:.88}"
    ".btn-secondary{background:var(--surface);color:var(--text);border:1px solid var(--border)}"
    ".btn-secondary:hover{background:var(--bg)}"
    ".btn-danger{background:#ef4444;color:#fff}"
    ".btn-danger:hover{background:#dc2626}"
    ".btn-edit{background:#f59e0b;color:#fff;display:inline-block;padding:.45rem 1.1rem;"
    "border-radius:.5rem;font-size:.9rem;font-weight:600;cursor:pointer;border:none;"
    "text-decoration:none;transition:.15s}"
    ".btn-edit:hover{background:#d97706}"
    ".img-preview{margin-top:.5rem;max-height:120px;border-radius:.5rem;"
    "border:1px solid var(--border)}"
    ".empty{text-align:center;padding:3rem;color:var(--muted)}"
)

THEME_JS = (
    "<script>\n"
    "(function(){\n"
    "  var saved=localStorage.getItem('es-theme')||'light';\n"
    "  document.documentElement.setAttribute('data-theme',saved);\n"
    "  var ico=document.getElementById('themeIco');\n"
    "  if(ico) ico.textContent=saved==='dark'?'☀️':'🌙';\n"
    "})();\n"
    "function toggleTheme(){\n"
    "  var root=document.documentElement;\n"
    "  var t=root.getAttribute('data-theme')==='dark'?'light':'dark';\n"
    "  root.setAttribute('data-theme',t);\n"
    "  localStorage.setItem('es-theme',t);\n"
    "  document.getElementById('themeIco').textContent=t==='dark'?'☀️':'🌙';\n"
    "}\n"
    "</script>"
)

HEADER_TMPL = (
    "<header class=\"hdr\">\n"
    "  <a href=\"/\" class=\"logo\">⚡ ElectroShop</a>\n"
    "  <input id=\"searchBox\" type=\"text\" placeholder=\"🔍 Tìm sản phẩm...\"\n"
    "         oninput=\"liveSearch(this.value)\" class=\"search-input\"\n"
    "         style=\"display:{{ 'flex' if active_tab=='shop' else 'none' }}\">\n"
    "  <nav class=\"hdr-nav\">\n"
    "    <a href=\"/\" class=\"{{ 'active' if active_tab=='shop' else '' }}\">🏪 Cửa hàng</a>\n"
    "    <a href=\"/news\" class=\"{{ 'active' if active_tab=='news' else '' }}\">📰 Tin tức</a>\n"
    "  </nav>\n"
    "  <div class=\"hdr-right\">\n"
    "    {% if active_tab == 'shop' %}\n"
    "    <a href=\"/add\"><button class=\"btn-hdr btn-add\">＋ Thêm SP</button></a>\n"
    "    {% else %}\n"
    "    <a href=\"/news/add\"><button class=\"btn-hdr btn-add\">✏️ Viết bài</button></a>\n"
    "    {% endif %}\n"
    "    <button class=\"btn-hdr btn-theme\" onclick=\"toggleTheme()\"><span id=\"themeIco\">🌙</span></button>\n"
    "  </div>\n"
    "</header>"
)


def _head(title):
    return (
        "<!DOCTYPE html>\n<html lang=\"vi\" data-theme=\"light\">\n<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>" + title + "</title>\n"
        "<style>" + SHARED_CSS + "</style>\n"
        + THEME_JS + "\n</head>\n<body>\n"
    )


LIVE_SEARCH_JS = (
    "<script>\n"
    "function liveSearch(q){\n"
    "  q=q.toLowerCase();\n"
    "  document.querySelectorAll('.card[data-s]').forEach(function(c){\n"
    "    c.style.display=c.dataset.s.includes(q)?'':'none';\n"
    "  });\n"
    "}\n"
    "</script>"
)

NOOP_SEARCH_JS = "<script>function liveSearch(){}</script>"

PREVIEW_IMG_JS = (
    "<script>\n"
    "function liveSearch(){}\n"
    "function previewImg(input){\n"
    "  var img=document.getElementById('imgPreview');\n"
    "  if(input.files&&input.files[0]){\n"
    "    var r=new FileReader();\n"
    "    r.onload=function(e){img.src=e.target.result;img.style.display='block';};\n"
    "    r.readAsDataURL(input.files[0]);\n"
    "  }\n"
    "}\n"
    "</script>"
)

# ── Page templates ────────────────────────────────────────────────────────────

PAGE_INDEX = (
    _head("ElectroShop – Cửa hàng")
    + HEADER_TMPL
    + """
<div class="container">
  <h1 class="page-title">🏪 Sản phẩm</h1>
  <p class="section-sub">Hàng chính hãng – bảo hành 12–24 tháng</p>
  {% if items %}
  <div class="grid">
    {% for p in items %}
    <a class="card" href="/chitiet?id={{ p['id'] }}"
       data-s="{{ p['name']|lower }} {{ p['category']|lower }}">
      <div class="card-img">
        {% if p['image'] %}
        <img src="/uploads/{{ p['image'] }}" alt="{{ p['name'] }}">
        {% else %}
        <span class="card-icon">{{ p['icon'] }}</span>
        {% endif %}
      </div>
      <div class="card-body">
        <div class="card-name">{{ p['name'] }}</div>
        <div class="card-price">{{ p['price'] }}</div>
        <div class="card-cat">{{ p['category'] }}</div>
        <div class="card-stock">Kho: {{ p['stock'] }}</div>
      </div>
    </a>
    {% endfor %}
  </div>
  {% else %}
  <div class="empty"><p>Chưa có sản phẩm. <a href="/add">Thêm ngay →</a></p></div>
  {% endif %}
</div>
"""
    + LIVE_SEARCH_JS
    + "\n</body></html>\n"
)

PAGE_CHITIET = (
    _head("{{ item['name'] }} – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <div class="detail-card">
    {% if item['image'] %}
    <img class="detail-img" src="/uploads/{{ item['image'] }}" alt="{{ item['name'] }}">
    {% else %}
    <span class="detail-icon">{{ item['icon'] }}</span>
    {% endif %}
    <div class="detail-name">{{ item['name'] }}</div>
    <div class="detail-price">{{ item['price'] }}</div>
    <p style="margin-bottom:.75rem">{{ item['description'] }}</p>
    <p style="margin-bottom:1.5rem">
      <span class="badge">{{ item['category'] }}</span>
      &nbsp;Kho: <strong>{{ item['stock'] }}</strong>
    </p>
    <div class="actions">
      <a href="/edit?id={{ item['id'] }}" class="btn-edit">✏️ Sửa</a>
      <button class="btn btn-danger" onclick="delProduct({{ item['id'] }})">🗑️ Xoá</button>
      <a href="/" class="btn btn-secondary">← Quay lại</a>
    </div>
  </div>
</div>
<script>
function liveSearch(){}
function delProduct(id){
  if(!confirm('Xoá sản phẩm này?')) return;
  fetch('/api/products/'+id,{method:'DELETE'})
    .then(function(r){return r.json();})
    .then(function(){location.href='/';})
    .catch(function(){alert('Lỗi khi xoá');});
}
</script>
</body></html>
"""
)

PAGE_ADD = (
    _head("Thêm sản phẩm – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <div class="form-card">
    <h2>➕ Thêm sản phẩm</h2>
    {% if error %}<p style="color:#ef4444;margin-bottom:1rem">{{ error }}</p>{% endif %}
    <form method="POST" action="/add" enctype="multipart/form-data">
      <div class="form-group">
        <label>Tên sản phẩm *</label>
        <input type="text" name="name" required placeholder="VD: iPhone 15 Pro">
      </div>
      <div class="form-group">
        <label>Giá *</label>
        <input type="text" name="price" required placeholder="VD: 27.990.000₫">
      </div>
      <div class="form-group">
        <label>Danh mục</label>
        <input type="text" name="category" placeholder="VD: Điện thoại">
      </div>
      <div class="form-group">
        <label>Mô tả</label>
        <textarea name="description" placeholder="Mô tả sản phẩm..."></textarea>
      </div>
      <div class="form-group">
        <label>Tồn kho</label>
        <input type="number" name="stock" value="0" min="0">
      </div>
      <div class="form-group">
        <label>Icon (emoji)</label>
        <input type="text" name="icon" value="📦" placeholder="📦">
      </div>
      <div class="form-group">
        <label>Ảnh sản phẩm</label>
        <input type="file" name="image" accept="image/*" onchange="previewImg(this)">
        <img id="imgPreview" class="img-preview" style="display:none">
      </div>
      <div class="actions">
        <button type="submit" class="btn btn-primary">💾 Lưu</button>
        <a href="/" class="btn btn-secondary">Huỷ</a>
      </div>
    </form>
  </div>
</div>
"""
    + PREVIEW_IMG_JS
    + "\n</body></html>\n"
)

PAGE_EDIT = (
    _head("Sửa sản phẩm – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <div class="form-card">
    <h2>✏️ Sửa sản phẩm</h2>
    {% if error %}<p style="color:#ef4444;margin-bottom:1rem">{{ error }}</p>{% endif %}
    <form method="POST" action="/edit?id={{ item['id'] }}" enctype="multipart/form-data">
      <div class="form-group">
        <label>Tên sản phẩm *</label>
        <input type="text" name="name" required value="{{ item['name'] }}">
      </div>
      <div class="form-group">
        <label>Giá *</label>
        <input type="text" name="price" required value="{{ item['price'] }}">
      </div>
      <div class="form-group">
        <label>Danh mục</label>
        <input type="text" name="category" value="{{ item['category'] }}">
      </div>
      <div class="form-group">
        <label>Mô tả</label>
        <textarea name="description">{{ item['description'] }}</textarea>
      </div>
      <div class="form-group">
        <label>Tồn kho</label>
        <input type="number" name="stock" value="{{ item['stock'] }}" min="0">
      </div>
      <div class="form-group">
        <label>Icon (emoji)</label>
        <input type="text" name="icon" value="{{ item['icon'] }}">
      </div>
      <div class="form-group">
        <label>Ảnh sản phẩm</label>
        {% if item['image'] %}
        <p style="margin-bottom:.4rem;font-size:.85rem;color:var(--muted)">Ảnh hiện tại:</p>
        <img src="/uploads/{{ item['image'] }}" class="img-preview" id="imgPreview">
        {% else %}
        <img id="imgPreview" class="img-preview" style="display:none">
        {% endif %}
        <input type="file" name="image" accept="image/*" onchange="previewImg(this)" style="margin-top:.5rem">
        <p style="font-size:.8rem;color:var(--muted);margin-top:.3rem">Chọn ảnh mới để thay thế ảnh cũ</p>
      </div>
      <div class="actions">
        <button type="submit" class="btn btn-primary">💾 Cập nhật</button>
        <a href="/chitiet?id={{ item['id'] }}" class="btn btn-secondary">Huỷ</a>
      </div>
    </form>
  </div>
</div>
"""
    + PREVIEW_IMG_JS
    + "\n</body></html>\n"
)

PAGE_NEWS = (
    _head("Tin tức – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <h1 class="page-title">📰 Tin tức</h1>
  <p class="section-sub">Cập nhật mới nhất về công nghệ và sản phẩm</p>
  {% if items %}
  <div class="news-grid">
    {% for n in items %}
    <a class="news-card" href="/news/{{ n['id'] }}">
      <div class="news-cover">
        {% if n['cover'] %}
        <img src="/uploads/{{ n['cover'] }}" alt="{{ n['title'] }}">
        {% else %}
        📰
        {% endif %}
      </div>
      <div class="news-body">
        <div class="news-title">{{ n['title'] }}</div>
        <div class="news-summary">{{ n['summary'] }}</div>
        <div class="news-meta">✍️ {{ n['author'] }} · {{ n['created_at'][:10] }}</div>
      </div>
    </a>
    {% endfor %}
  </div>
  {% else %}
  <div class="empty"><p>Chưa có bài viết. <a href="/news/add">Viết bài →</a></p></div>
  {% endif %}
</div>
"""
    + NOOP_SEARCH_JS
    + "\n</body></html>\n"
)

PAGE_NEWS_DETAIL = (
    _head("{{ article['title'] }} – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <div class="detail-card">
    <div class="article-title">{{ article['title'] }}</div>
    <div class="article-meta">✍️ {{ article['author'] }} · {{ article['created_at'][:10] }}</div>
    {% if article['cover'] %}
    <img class="article-cover" src="/uploads/{{ article['cover'] }}" alt="{{ article['title'] }}">
    {% else %}
    <div class="article-cover-ph">📰</div>
    {% endif %}
    <div class="article-content">{{ article['content'] }}</div>
    <div class="actions" style="margin-top:2rem">
      <button class="btn btn-danger" onclick="delNews({{ article['id'] }})">🗑️ Xoá</button>
      <a href="/news" class="btn btn-secondary">← Tin tức</a>
    </div>
  </div>
</div>
<script>
function liveSearch(){}
function delNews(id){
  if(!confirm('Xoá bài viết này?')) return;
  fetch('/api/news/'+id,{method:'DELETE'})
    .then(function(r){return r.json();})
    .then(function(){location.href='/news';})
    .catch(function(){alert('Lỗi khi xoá');});
}
</script>
</body></html>
"""
)

PAGE_NEWS_ADD = (
    _head("Viết bài – ElectroShop")
    + HEADER_TMPL
    + """
<div class="container">
  <div class="form-card">
    <h2>✏️ Viết bài mới</h2>
    {% if error %}<p style="color:#ef4444;margin-bottom:1rem">{{ error }}</p>{% endif %}
    <form method="POST" action="/news/add" enctype="multipart/form-data">
      <div class="form-group">
        <label>Tiêu đề *</label>
        <input type="text" name="title" required placeholder="Tiêu đề bài viết...">
      </div>
      <div class="form-group">
        <label>Tóm tắt</label>
        <input type="text" name="summary" placeholder="Một dòng tóm tắt...">
      </div>
      <div class="form-group">
        <label>Nội dung *</label>
        <textarea name="content" required placeholder="Nội dung bài viết..." style="min-height:180px"></textarea>
      </div>
      <div class="form-group">
        <label>Tác giả</label>
        <input type="text" name="author" value="Admin">
      </div>
      <div class="form-group">
        <label>Ảnh bìa</label>
        <input type="file" name="cover" accept="image/*" onchange="previewImg(this)">
        <img id="imgPreview" class="img-preview" style="display:none">
      </div>
      <div class="actions">
        <button type="submit" class="btn btn-primary">📤 Đăng bài</button>
        <a href="/news" class="btn btn-secondary">Huỷ</a>
      </div>
    </form>
  </div>
</div>
"""
    + PREVIEW_IMG_JS
    + "\n</body></html>\n"
)

# ── Routes ────────────────────────────────────────────────────────────────────

_ROOT = os.path.dirname(os.path.abspath(__file__))

# ─── Serve root-level static files (index.html, news.html, cart.js, api.js…) ─

@app.route('/')
def index():
    """Serve the SPA index.html if present, else fall back to Jinja template."""
    html = os.path.join(_ROOT, 'index.html')
    if os.path.exists(html):
        return send_from_directory(_ROOT, 'index.html')
    # Fallback: Jinja template (admin/Pi local view)
    items = fetch_from_pocketbase()
    if not items:
        try:
            with get_db() as conn:
                rows = conn.execute("SELECT * FROM products ORDER BY created_at DESC").fetchall()
                items = [normalize_product(r) for r in rows]
        except Exception:
            items = []
    return render_template_string(PAGE_INDEX, items=items, active_tab='shop')


@app.route('/chitiet')
def chitiet():
    pid = request.args.get('id', '').strip()
    if not pid:
        abort(404)

    # 1) Try PocketBase
    items = fetch_from_pocketbase()
    item = next((p for p in items if str(p['id']) == str(pid)), None)

    # 2) Fallback SQLite
    if not item:
        try:
            with get_db() as conn:
                row = conn.execute("SELECT * FROM products WHERE id = ?", (int(pid),)).fetchone()
                if row:
                    item = normalize_product(row)
        except Exception as e:
            print(f'[chitiet/SQLite] {e}')

    if not item:
        abort(404)
    return render_template_string(PAGE_CHITIET, item=item, active_tab='shop')


@app.route('/add', methods=['GET', 'POST'])
def add():
    if request.method == 'GET':
        return render_template_string(PAGE_ADD, error=None, active_tab='shop')
    name = request.form.get('name', '').strip()
    price = request.form.get('price', '').strip()
    if not name or not price:
        return render_template_string(PAGE_ADD, error='Vui lòng điền tên và giá sản phẩm.', active_tab='shop')
    category = request.form.get('category', '').strip()
    description = request.form.get('description', '').strip()
    stock = request.form.get('stock', 0)
    icon = request.form.get('icon', '📦').strip() or '📦'
    image = save_upload(request.files.get('image'))
    with get_db() as c:
        c.execute(
            'INSERT INTO products(name,price,category,description,stock,icon,image) VALUES(?,?,?,?,?,?,?)',
            (name, price, category, description, stock, icon, image)
        )
        c.commit()
    return redirect('/')


@app.route('/edit', methods=['GET', 'POST'])
def edit():
    pid = request.args.get('id', type=int)
    if not pid:
        abort(404)
    with get_db() as c:
        item = c.execute('SELECT * FROM products WHERE id=?', (pid,)).fetchone()
    if not item:
        abort(404)
    if request.method == 'GET':
        return render_template_string(PAGE_EDIT, item=item, error=None, active_tab='shop')
    name = request.form.get('name', '').strip()
    price = request.form.get('price', '').strip()
    if not name or not price:
        return render_template_string(PAGE_EDIT, item=item, error='Vui lòng điền tên và giá.', active_tab='shop')
    category = request.form.get('category', '').strip()
    description = request.form.get('description', '').strip()
    stock = request.form.get('stock', 0)
    icon = request.form.get('icon', '📦').strip() or '📦'
    new_image = save_upload(request.files.get('image'))
    if new_image:
        delete_image(item['image'])
        image = new_image
    else:
        image = item['image']
    with get_db() as c:
        c.execute(
            'UPDATE products SET name=?,price=?,category=?,description=?,stock=?,icon=?,image=? WHERE id=?',
            (name, price, category, description, stock, icon, image, pid)
        )
        c.commit()
    return redirect('/')


@app.route('/api/products/<int:pid>', methods=['DELETE'])
def api_delete_product(pid):
    with get_db() as c:
        item = c.execute('SELECT * FROM products WHERE id=?', (pid,)).fetchone()
        if not item:
            abort(404)
        delete_image(item['image'])
        c.execute('DELETE FROM products WHERE id=?', (pid,))
        c.commit()
    return jsonify({'ok': True, 'id': pid})


@app.route('/uploads/<filename>')
def uploads(filename):
    return send_from_directory(UPLOAD_DIR, filename)


IMAGES_DIR = os.environ.get('IMAGES_DIR', IMAGES_DIR_DEFAULT)
os.makedirs(IMAGES_DIR, exist_ok=True)

@app.route('/images/<path:filename>')
def serve_image(filename):
    return send_from_directory(IMAGES_DIR, filename)

@app.route('/api/upload', methods=['POST'])
def api_upload():
    f = request.files.get('file') or request.files.get('image')
    if not f or not f.filename:
        return jsonify({'error': 'Không có file'}), 400
    ext = f.filename.rsplit('.', 1)[-1].lower() if '.' in f.filename else ''
    if ext not in ALLOWED_EXT:
        return jsonify({'error': f'Định dạng không hỗ trợ: {ext}'}), 400
    # Giữ tên gốc nhưng làm sạch, thêm timestamp tránh trùng
    import time, re
    safe = re.sub(r'[^\w\-.]', '_', f.filename.rsplit('.', 1)[0])[:40]
    fname = f'{safe}_{int(time.time())}.{ext}'
    f.save(os.path.join(IMAGES_DIR, fname))
    url = f'/images/{fname}'
    return jsonify({'url': url, 'filename': fname, 'full_url': f'https://electroshop-pi5.electroshop-tho.workers.dev{url}'})


@app.route('/api/product')
def api_product():
    """Get a single product by id — used by api.js fetchProduct() on Vercel."""
    pid = request.args.get('id', '').strip()
    if not pid:
        return jsonify({'error': 'Thiếu id'}), 400

    # 1) Try PocketBase first
    items = fetch_from_pocketbase()
    item = next((p for p in items if str(p['id']) == str(pid)), None)
    if item:
        return jsonify(item)

    # 2) Fallback: SQLite
    try:
        with get_db() as conn:
            row = conn.execute("SELECT * FROM products WHERE id = ?", (int(pid),)).fetchone()
            if row:
                return jsonify(normalize_product(row))
    except Exception as e:
        print(f'[SQLite/product] {e}')

    return jsonify({'error': 'Không tìm thấy sản phẩm'}), 404


@app.route('/api/products', methods=['GET', 'POST'])
def api_products():
    if request.method == 'POST':
        d = request.get_json(silent=True) or {}
        name  = (d.get('name') or '').strip()
        price = d.get('price')
        if not name or not price:
            return jsonify({'error': 'Thiếu name hoặc price'}), 400

        # 1) Try PocketBase
        result = add_to_pocketbase({
            'name': name, 'price': price,
            'description': d.get('description', ''),
            'category': d.get('category', ''),
            'stock': d.get('stock', 0),
            'icon': d.get('icon', '📦'),
        })
        if result:
            return jsonify(result), 201

        # 2) Fallback: SQLite
        try:
            with get_db() as c:
                c.execute(
                    'INSERT INTO products(name,price,category,description,stock,icon) VALUES(?,?,?,?,?,?)',
                    (name, str(price), d.get('category', ''), d.get('description', ''),
                     int(d.get('stock', 0)), d.get('icon', '📦'))
                )
                c.commit()
                row = c.execute("SELECT * FROM products WHERE rowid = last_insert_rowid()").fetchone()
                return jsonify(normalize_product(row)), 201
        except Exception as e:
            print(f'[SQLite/insert] {e}')
            return jsonify({'error': 'Không thể thêm sản phẩm'}), 500

    # GET: Try PocketBase first, fallback to SQLite
    products = fetch_from_pocketbase()
    if products:
        return jsonify(products)

    try:
        with get_db() as conn:
            rows = conn.execute("SELECT * FROM products ORDER BY created_at DESC").fetchall()
            return jsonify([normalize_product(r) for r in rows])
    except Exception as e:
        print(f'[SQLite/list] {e}')
        return jsonify([]), 200


@app.route('/news')
def news_list():
    with get_db() as c:
        items = c.execute('SELECT * FROM news ORDER BY created_at DESC').fetchall()
    return render_template_string(PAGE_NEWS, items=items, active_tab='news')


@app.route('/news/add', methods=['GET', 'POST'])
def news_add():
    if request.method == 'GET':
        return render_template_string(PAGE_NEWS_ADD, error=None, active_tab='news')
    title = request.form.get('title', '').strip()
    content = request.form.get('content', '').strip()
    if not title or not content:
        return render_template_string(PAGE_NEWS_ADD, error='Vui lòng điền tiêu đề và nội dung.', active_tab='news')
    summary = request.form.get('summary', '').strip()
    author = request.form.get('author', 'Admin').strip() or 'Admin'
    cover = save_upload(request.files.get('cover'))
    with get_db() as c:
        c.execute(
            'INSERT INTO news(title,content,summary,cover,author) VALUES(?,?,?,?,?)',
            (title, content, summary, cover, author)
        )
        c.commit()
    return redirect('/news')


@app.route('/news/<int:nid>')
def news_detail(nid):
    with get_db() as c:
        article = c.execute('SELECT * FROM news WHERE id=?', (nid,)).fetchone()
    if not article:
        abort(404)
    return render_template_string(PAGE_NEWS_DETAIL, article=article, active_tab='news')


@app.route('/api/news/<int:nid>', methods=['DELETE'])
def api_delete_news(nid):
    with get_db() as c:
        article = c.execute('SELECT * FROM news WHERE id=?', (nid,)).fetchone()
        if not article:
            abort(404)
        delete_image(article['cover'])
        c.execute('DELETE FROM news WHERE id=?', (nid,))
        c.commit()
    return jsonify({'ok': True, 'id': nid})


@app.route('/api/news')
def api_news():
    with get_db() as c:
        rows = c.execute(
            'SELECT id, title, summary, content, author, created_at, cover FROM news ORDER BY created_at DESC'
        ).fetchall()
    result = []
    for r in rows:
        item = dict(r)
        item['image_url'] = item.get('cover', '')   # alias cho frontend dễ dùng
        result.append(item)
    return jsonify(result)


@app.route('/health')
def health():
    return jsonify({'status': 'ok'})


# ── Payment (Sacombank VietQR + Sepay webhook) ────────────────────────────────

_PAY_BANK_ID      = 'STB'
_PAY_ACCOUNT_NO   = '0355020289'
_PAY_ACCOUNT_NAME = 'DO THI THU HA'
_SEPAY_SECRET     = os.environ.get('SEPAY_SECRET', 'YOUR_SEPAY_WEBHOOK_SECRET')

_orders      = {}
_orders_lock = threading.Lock()

def _auto_clean_orders():
    while True:
        time.sleep(3600)
        cutoff = time.time() - 86400
        with _orders_lock:
            for oid in list(_orders.keys()):
                if _orders[oid]['createdAt'] < cutoff:
                    del _orders[oid]

threading.Thread(target=_auto_clean_orders, daemon=True).start()


@app.route('/api/create-order')
def api_create_order():
    amount  = int(request.args.get('amount', 50000))
    product = request.args.get('product', '')
    oid     = 'DH' + str(int(time.time() * 1000))[-8:]
    content = f'THANHTOAN {oid}'
    qr_url  = (
        f'https://img.vietqr.io/image/{_PAY_BANK_ID}-{_PAY_ACCOUNT_NO}-compact2.png'
        f'?amount={amount}&addInfo={quote(content)}&accountName={quote(_PAY_ACCOUNT_NAME)}'
    )
    with _orders_lock:
        _orders[oid] = {
            'orderId': oid, 'amount': amount, 'content': content,
            'product': product, 'status': 'pending', 'createdAt': time.time()
        }
    return jsonify({
        'success': True, 'orderId': oid, 'amount': amount,
        'content': content, 'qrUrl': qr_url, 'product': product,
        'accountNo': _PAY_ACCOUNT_NO, 'accountName': _PAY_ACCOUNT_NAME, 'bankId': _PAY_BANK_ID
    })


@app.route('/api/order-status')
def api_order_status():
    oid = request.args.get('orderId', '')
    with _orders_lock:
        order = _orders.get(oid)
    if not order:
        return jsonify({'success': False, 'message': 'Không tìm thấy đơn hàng'}), 404
    return jsonify({'success': True, **order})


@app.route('/api/deliver', methods=['POST', 'OPTIONS'])
def api_deliver():
    if request.method == 'OPTIONS':
        return '', 204
    data  = request.get_json() or {}
    oid   = data.get('orderId') or request.args.get('orderId', '')
    with _orders_lock:
        order = _orders.get(oid)
        if not order:
            return jsonify({'success': False, 'message': 'Không tìm thấy đơn hàng'}), 404
        if order['status'] != 'paid':
            return jsonify({'success': False, 'message': 'Đơn chưa thanh toán'}), 400
        order['status']      = 'delivered'
        order['deliveredAt'] = time.time()
    return jsonify({'success': True, **order})


@app.route('/webhook/sepay', methods=['POST'])
def webhook_sepay():
    data = request.get_json() or {}
    if (_SEPAY_SECRET != 'YOUR_SEPAY_WEBHOOK_SECRET'
            and request.headers.get('apikey', '') != _SEPAY_SECRET):
        return jsonify({'success': False}), 401

    content  = (data.get('content') or data.get('code') or '').upper()
    ttype    = data.get('transferType', '')
    amount   = data.get('transferAmount', 0)

    if ttype != 'in':
        return jsonify({'success': True, 'message': 'Bỏ qua'})

    with _orders_lock:
        for order in _orders.values():
            if order['orderId'].upper() in content and order['status'] == 'pending':
                if amount >= order['amount']:
                    order['status']    = 'paid'
                    order['paidAt']    = time.time()
                    order['paidAmount']= amount
                break
    return jsonify({'success': True})


@app.route('/api/orders')
def api_orders():
    with _orders_lock:
        lst = sorted(_orders.values(), key=lambda x: x['createdAt'], reverse=True)
    return jsonify({'success': True, 'orders': list(lst)})


# ── Main ──────────────────────────────────────────────────────────────────────


# ── Catch-all: serve any static file from project root (last resort) ──────────
# Must be registered AFTER all other routes to avoid shadowing them.

@app.route('/<path:filename>')
def serve_root_file(filename):
    """Serves index.html, news.html, cart.js, api.js, chitiet.html, etc.
    from the project root directory."""
    filepath = os.path.join(_ROOT, filename)
    if os.path.isfile(filepath):
        return send_from_directory(_ROOT, filename)
    abort(404)


if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=8888, debug=False)
else:
    # Vercel serverless: init on cold start
    init_db()
