from flask import Flask, request, redirect, render_template_string, jsonify, abort
from werkzeug.middleware.proxy_fix import ProxyFix
import sqlite3, os

app = Flask(__name__)
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1)

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'backup.db')

# ── DB helpers ────────────────────────────────────────────────────────────────
def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    with get_db() as conn:
        conn.execute('''
            CREATE TABLE IF NOT EXISTS products (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                name        TEXT    NOT NULL,
                price       TEXT    NOT NULL,
                category    TEXT    DEFAULT '',
                description TEXT    DEFAULT '',
                stock       INTEGER DEFAULT 0,
                icon        TEXT    DEFAULT '📦',
                created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )''')
        conn.commit()
        # Seed mẫu nếu bảng trống
        if conn.execute('SELECT COUNT(*) FROM products').fetchone()[0] == 0:
            conn.executemany(
                'INSERT INTO products (name,price,category,description,stock,icon) VALUES (?,?,?,?,?,?)',
                [
                    ('iPhone 15 Pro', '27.990.000₫', 'Điện thoại', 'Chip A17 Pro, camera 48MP, Dynamic Island', 10, '📱'),
                    ('MacBook Air M3', '28.990.000₫', 'Laptop',     'M3 chip, 8GB RAM, 256GB SSD, 18h pin',    5,  '💻'),
                    ('AirPods Pro 2', '6.490.000₫',  'Tai nghe',   'Active Noise Cancellation, USB-C',         20, '🎧'),
                ]
            )
            conn.commit()

# ── Shared CSS & JS (nhúng inline để 1 file duy nhất) ────────────────────────
BASE_STYLE = """
<style>
:root{
  --bg:#f0f4f8;--surface:#fff;--text:#1a202c;--text-muted:#718096;
  --border:#e2e8f0;--shadow:0 2px 8px rgba(0,0,0,.08);--radius:12px;
  --grad-a:#6366f1;--grad-b:#8b5cf6;--danger:#ef4444;
}
[data-theme=dark]{
  --bg:#0f1117;--surface:#1a1d27;--text:#e2e8f0;--text-muted:#a0aec0;
  --border:#2d3748;--shadow:0 2px 12px rgba(0,0,0,.4);
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh}

/* Header */
.header{background:linear-gradient(135deg,var(--grad-a),var(--grad-b));
  padding:0 24px;display:flex;align-items:center;gap:16px;
  height:60px;position:sticky;top:0;z-index:100;
  box-shadow:0 2px 12px rgba(99,102,241,.4)}
.logo{color:#fff;font-size:1.2rem;font-weight:700;text-decoration:none;white-space:nowrap}
.search-wrap{flex:1;max-width:480px}
.search-input{width:100%;padding:8px 16px;border-radius:999px;border:none;
  background:rgba(255,255,255,.2);color:#fff;font-size:.95rem;outline:none}
.search-input::placeholder{color:rgba(255,255,255,.65)}
.search-input:focus{background:rgba(255,255,255,.3)}
.header-actions{display:flex;gap:10px;align-items:center;margin-left:auto}
.btn-add{background:#fff;color:var(--grad-a);border:none;border-radius:8px;
  padding:8px 16px;font-weight:600;cursor:pointer;font-size:.9rem;white-space:nowrap}
.btn-add:hover{background:#ede9fe}
.theme-btn{background:rgba(255,255,255,.2);border:none;border-radius:8px;
  padding:8px 12px;color:#fff;cursor:pointer;font-size:1rem}

/* Main */
.main{max-width:1200px;margin:0 auto;padding:24px 16px}

/* Grid */
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:20px;margin-top:20px}
.card{background:var(--surface);border-radius:var(--radius);border:1px solid var(--border);
  box-shadow:var(--shadow);overflow:hidden;cursor:pointer;
  transition:transform .2s,box-shadow .2s;display:flex;flex-direction:column}
.card:hover{transform:translateY(-4px);box-shadow:0 8px 24px rgba(99,102,241,.2)}
.card-img{height:130px;background:linear-gradient(135deg,#ede9fe,#ddd6fe);
  display:flex;align-items:center;justify-content:center;font-size:3.5rem}
.card-body{padding:16px;flex:1;display:flex;flex-direction:column;gap:6px}
.card-name{font-weight:700;font-size:1rem;line-height:1.3}
.card-cat{font-size:.78rem;color:#fff;background:var(--grad-a);
  display:inline-block;padding:2px 10px;border-radius:999px;width:fit-content}
.card-desc{font-size:.85rem;color:var(--text-muted);flex:1}
.card-footer{display:flex;align-items:center;justify-content:space-between;margin-top:8px}
.card-price{font-weight:700;font-size:1.05rem;
  background:linear-gradient(135deg,var(--grad-a),var(--grad-b));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent}
.btn-detail{background:linear-gradient(135deg,var(--grad-a),var(--grad-b));
  color:#fff;border:none;border-radius:8px;padding:6px 14px;
  font-size:.8rem;cursor:pointer}
.card-stock{font-size:.78rem;color:var(--text-muted);padding:0 16px 12px}

/* Section title */
.section-title{font-size:1.5rem;font-weight:700;margin-bottom:4px}
.section-sub{color:var(--text-muted);font-size:.9rem}

/* Detail page */
.detail-wrap{max-width:800px;margin:32px auto;padding:0 16px}
.detail-card{background:var(--surface);border-radius:var(--radius);
  border:1px solid var(--border);box-shadow:var(--shadow);overflow:hidden}
.detail-img{height:220px;background:linear-gradient(135deg,#ede9fe,#ddd6fe);
  display:flex;align-items:center;justify-content:center;font-size:6rem}
.detail-body{padding:28px;display:flex;flex-direction:column;gap:12px}
.detail-name{font-size:1.6rem;font-weight:700}
.detail-price{font-size:1.4rem;font-weight:700;
  background:linear-gradient(135deg,var(--grad-a),var(--grad-b));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent}
.detail-cat{display:inline-block;background:var(--grad-a);color:#fff;
  padding:4px 14px;border-radius:999px;font-size:.85rem}
.detail-desc{color:var(--text-muted);line-height:1.6}
.detail-stock{color:var(--text-muted);font-size:.9rem}
.detail-meta{font-size:.8rem;color:var(--text-muted)}
.btn-back{display:inline-block;margin:20px 0;padding:10px 20px;
  background:var(--surface);border:1px solid var(--border);border-radius:8px;
  color:var(--text);text-decoration:none;font-weight:600}
.btn-back:hover{background:var(--bg)}

/* Add form */
.form-wrap{max-width:600px;margin:32px auto;padding:0 16px}
.form-card{background:var(--surface);border-radius:var(--radius);
  border:1px solid var(--border);box-shadow:var(--shadow);padding:28px}
.form-title{font-size:1.4rem;font-weight:700;margin-bottom:20px}
.form-group{display:flex;flex-direction:column;gap:6px;margin-bottom:16px}
label{font-size:.85rem;font-weight:600;color:var(--text-muted);text-transform:uppercase;letter-spacing:.05em}
input,select,textarea{padding:10px 14px;border-radius:8px;border:1px solid var(--border);
  background:var(--bg);color:var(--text);font-size:.95rem;width:100%;outline:none}
input:focus,select:focus,textarea:focus{border-color:var(--grad-a);box-shadow:0 0 0 3px rgba(99,102,241,.15)}
textarea{min-height:90px;resize:vertical}
.btn-submit{width:100%;padding:12px;background:linear-gradient(135deg,var(--grad-a),var(--grad-b));
  color:#fff;border:none;border-radius:8px;font-size:1rem;font-weight:700;cursor:pointer;margin-top:8px}
.btn-submit:hover{opacity:.9}
.flash{padding:12px 16px;border-radius:8px;margin-bottom:16px;font-size:.9rem}
.flash.error{background:#fee2e2;color:#991b1b;border:1px solid #fca5a5}
.flash.success{background:#d1fae5;color:#065f46;border:1px solid #6ee7b7}

/* Empty */
.empty{text-align:center;padding:60px 20px;color:var(--text-muted)}
.empty-icon{font-size:3rem;margin-bottom:12px}

/* Responsive */
@media(max-width:600px){
  .header{padding:0 12px;gap:10px;height:56px}
  .logo{font-size:1rem}
  .grid{grid-template-columns:1fr 1fr}
}
@media(max-width:400px){
  .grid{grid-template-columns:1fr}
}
</style>
<script>
function toggleTheme(){
  const t=document.documentElement.getAttribute('data-theme')==='dark'?'light':'dark';
  document.documentElement.setAttribute('data-theme',t);
  localStorage.setItem('es-backup-theme',t);
  document.getElementById('themeIcon').textContent=t==='dark'?'☀️':'🌙';
}
(function(){
  const s=localStorage.getItem('es-backup-theme');
  if(s){document.documentElement.setAttribute('data-theme',s);}
  else if(window.matchMedia('(prefers-color-scheme:dark)').matches)
    document.documentElement.setAttribute('data-theme','dark');
})();
</script>
"""

HEADER_HTML = """
<header class="header">
  <a href="/" class="logo">⚡ ElectroShop <span style="font-size:.7rem;opacity:.7">Backup</span></a>
  <div class="search-wrap">
    <input class="search-input" id="searchInput" type="text"
           placeholder="🔍 Tìm sản phẩm..." oninput="liveSearch(this.value)">
  </div>
  <div class="header-actions">
    <a href="/add"><button class="btn-add">＋ Thêm</button></a>
    <button class="theme-btn" onclick="toggleTheme()"><span id="themeIcon">🌙</span></button>
  </div>
</header>
<script>
function liveSearch(q){
  q=q.trim().toLowerCase();
  document.querySelectorAll('.card').forEach(c=>{
    c.style.display=(!q||c.dataset.search.includes(q))?'':'none';
  });
}
</script>
"""

# ── Templates ─────────────────────────────────────────────────────────────────
INDEX_TMPL = BASE_STYLE + HEADER_HTML + """
<div class="main">
  <div class="section-title">Danh sách sản phẩm</div>
  <div class="section-sub">{{ products|length }} sản phẩm — bấm để xem chi tiết</div>
  {% if products %}
  <div class="grid" id="grid">
    {% for p in products %}
    <div class="card" data-search="{{ (p.name~' '~p.category~' '~p.description)|lower }}"
         onclick="location.href='/chitiet?id={{ p.id }}'">
      <div class="card-img">{{ p.icon }}</div>
      <div class="card-body">
        <div class="card-name">{{ p.name }}</div>
        {% if p.category %}<span class="card-cat">{{ p.category }}</span>{% endif %}
        <div class="card-desc">{{ p.description or '—' }}</div>
        <div class="card-footer">
          <div class="card-price">{{ p.price }}</div>
          <button class="btn-detail"
            onclick="event.stopPropagation();location.href='/chitiet?id={{ p.id }}'">
            Chi tiết →
          </button>
        </div>
      </div>
      <div class="card-stock">📦 Kho: {{ p.stock }} cái</div>
    </div>
    {% endfor %}
  </div>
  {% else %}
  <div class="empty">
    <div class="empty-icon">📭</div>
    <p>Chưa có sản phẩm. <a href="/add" style="color:var(--grad-a)">Thêm ngay →</a></p>
  </div>
  {% endif %}
</div>
"""

DETAIL_TMPL = BASE_STYLE + HEADER_HTML + """
<div class="detail-wrap">
  <a href="/" class="btn-back">← Quay lại danh sách</a>
  <div class="detail-card">
    <div class="detail-img">{{ p.icon }}</div>
    <div class="detail-body">
      <div class="detail-name">{{ p.name }}</div>
      <div class="detail-price">{{ p.price }}</div>
      {% if p.category %}<span class="detail-cat">{{ p.category }}</span>{% endif %}
      {% if p.description %}
      <div class="detail-desc">{{ p.description }}</div>
      {% endif %}
      <div class="detail-stock">📦 Tồn kho: <strong>{{ p.stock }}</strong> cái</div>
      <div class="detail-meta">🕒 Thêm lúc: {{ p.created_at }}</div>
      <div style="display:flex;gap:12px;margin-top:12px">
        <a href="/" class="btn-back" style="margin:0">← Quay lại</a>
        <button onclick="deleteProduct({{ p.id }})"
          style="padding:10px 20px;background:var(--danger);color:#fff;border:none;
                 border-radius:8px;cursor:pointer;font-weight:600">
          🗑 Xóa sản phẩm
        </button>
      </div>
    </div>
  </div>
</div>
<script>
async function deleteProduct(id){
  if(!confirm('Xóa sản phẩm này?')) return;
  const r=await fetch('/api/products/'+id,{method:'DELETE'});
  const d=await r.json();
  if(d.ok) location.href='/';
  else alert('Lỗi: '+d.error);
}
</script>
"""

ADD_TMPL = BASE_STYLE + HEADER_HTML + """
<div class="form-wrap">
  <a href="/" class="btn-back">← Quay lại danh sách</a>
  <div class="form-card">
    <div class="form-title">➕ Thêm sản phẩm mới</div>
    {% if msg %}
    <div class="flash {{ msg_type }}">{{ msg }}</div>
    {% endif %}
    <form method="POST" action="/add">
      <div class="form-group">
        <label>Tên sản phẩm *</label>
        <input name="name" required placeholder="VD: iPhone 15 Pro" value="{{ form.name }}">
      </div>
      <div class="form-group">
        <label>Giá *</label>
        <input name="price" required placeholder="VD: 27.990.000₫" value="{{ form.price }}">
      </div>
      <div class="form-group">
        <label>Danh mục</label>
        <select name="category">
          {% set cats = ['Điện thoại','Laptop','Tai nghe','Đồng hồ','Máy ảnh','TV','Game','Máy tính bảng','Khác'] %}
          <option value="">-- Chọn danh mục --</option>
          {% for c in cats %}
          <option value="{{ c }}" {% if form.category==c %}selected{% endif %}>{{ c }}</option>
          {% endfor %}
        </select>
      </div>
      <div class="form-group">
        <label>Icon (emoji)</label>
        <input name="icon" placeholder="📦" maxlength="8" value="{{ form.icon or '📦' }}">
      </div>
      <div class="form-group">
        <label>Mô tả</label>
        <textarea name="description" placeholder="Mô tả ngắn về sản phẩm...">{{ form.description }}</textarea>
      </div>
      <div class="form-group">
        <label>Số lượng tồn kho</label>
        <input type="number" name="stock" min="0" value="{{ form.stock or 0 }}">
      </div>
      <button type="submit" class="btn-submit">💾 Lưu sản phẩm</button>
    </form>
  </div>
</div>
"""

# ── Routes ────────────────────────────────────────────────────────────────────
@app.route('/')
def index():
    try:
        with get_db() as conn:
            rows = conn.execute(
                'SELECT * FROM products ORDER BY id DESC'
            ).fetchall()
        return render_template_string(INDEX_TMPL, products=rows)
    except Exception as e:
        return f'<pre>DB Error: {e}</pre>', 500

@app.route('/chitiet')
def chitiet():
    pid = request.args.get('id', type=int)
    try:
        with get_db() as conn:
            p = conn.execute('SELECT * FROM products WHERE id=?', (pid,)).fetchone()
        if p is None:
            abort(404)
        return render_template_string(DETAIL_TMPL, p=p)
    except Exception as e:
        return f'<pre>Error: {e}</pre>', 500

@app.route('/add', methods=['GET'])
def add_get():
    empty = dict(name='', price='', category='', icon='📦', description='', stock=0)
    return render_template_string(ADD_TMPL, msg=None, msg_type='', form=empty)

@app.route('/add', methods=['POST'])
def add_post():
    name        = request.form.get('name', '').strip()
    price       = request.form.get('price', '').strip()
    category    = request.form.get('category', '').strip()
    icon        = request.form.get('icon', '📦').strip() or '📦'
    description = request.form.get('description', '').strip()
    stock       = request.form.get('stock', '0').strip()
    form        = dict(name=name, price=price, category=category,
                       icon=icon, description=description, stock=stock)
    if not name or not price:
        return render_template_string(ADD_TMPL, msg='⚠️ Tên và giá không được để trống.',
                                      msg_type='error', form=form), 400
    try:
        stock_int = max(0, int(stock))
    except ValueError:
        stock_int = 0
    try:
        with get_db() as conn:
            conn.execute(
                'INSERT INTO products (name,price,category,description,stock,icon) VALUES (?,?,?,?,?,?)',
                (name, price, category, description, stock_int, icon)
            )
            conn.commit()
    except Exception as e:
        return render_template_string(ADD_TMPL, msg=f'Lỗi DB: {e}',
                                      msg_type='error', form=form), 500
    return redirect('/')

@app.route('/api/products', methods=['GET'])
def api_products():
    try:
        with get_db() as conn:
            rows = conn.execute(
                'SELECT id,name,price,category,description,stock,icon,created_at FROM products ORDER BY id DESC'
            ).fetchall()
        return jsonify([dict(r) for r in rows])
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/products/<int:pid>', methods=['DELETE'])
def api_delete(pid):
    try:
        with get_db() as conn:
            r = conn.execute('DELETE FROM products WHERE id=?', (pid,))
            conn.commit()
        if r.rowcount == 0:
            return jsonify({'ok': False, 'error': 'Không tìm thấy'}), 404
        return jsonify({'ok': True})
    except Exception as e:
        return jsonify({'ok': False, 'error': str(e)}), 500

@app.route('/health')
def health():
    try:
        with get_db() as conn:
            conn.execute('SELECT 1')
        return jsonify({'status': 'ok', 'db': 'connected', 'port': 8090})
    except Exception as e:
        return jsonify({'status': 'error', 'db': str(e)}), 500

# ── Entry point ───────────────────────────────────────────────────────────────
if __name__ == '__main__':
    init_db()
    print('🚀 ElectroShop Backup chạy tại http://0.0.0.0:8090')
    app.run(host='0.0.0.0', port=8090, debug=False)
