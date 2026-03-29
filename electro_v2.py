from flask import Flask, request, redirect, render_template_string, jsonify, abort, send_from_directory
from flask_cors import CORS
from werkzeug.middleware.proxy_fix import ProxyFix
from werkzeug.utils import secure_filename
import sqlite3, os, uuid

app = Flask(__name__)
CORS(app)
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1)

@app.after_request
def add_cors(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Methods'] = 'GET, POST, PUT, DELETE, OPTIONS'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type, Authorization'
    return response

DB = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'v2.db')
UPLOAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'uploads')
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

@app.route('/')
def index():
    with get_db() as c:
        items = c.execute('SELECT * FROM products ORDER BY created_at DESC').fetchall()
    return render_template_string(PAGE_INDEX, items=items, active_tab='shop')


@app.route('/chitiet')
def chitiet():
    pid = request.args.get('id', type=int)
    if not pid:
        abort(404)
    with get_db() as c:
        item = c.execute('SELECT * FROM products WHERE id=?', (pid,)).fetchone()
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


@app.route('/api/products')
def api_products():
    with get_db() as c:
        rows = c.execute('SELECT * FROM products ORDER BY created_at DESC').fetchall()
    return jsonify([dict(r) for r in rows])


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
            'SELECT id, title, summary, author, created_at, cover FROM news ORDER BY created_at DESC'
        ).fetchall()
    return jsonify([dict(r) for r in rows])


@app.route('/health')
def health():
    return jsonify({'status': 'ok'})


# ── Main ──────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=8888, debug=False)
