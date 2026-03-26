#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

WebServer webServer(80);
bool webServerRunning = false;

// ---------------------------------------------------------------------------
// HTML Pages (stored in flash via PROGMEM)
// ---------------------------------------------------------------------------

static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ElectroShop Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh;padding:20px}
h1{color:#00ffff;text-align:center;margin-bottom:8px;font-size:1.6rem}
.sub{text-align:center;color:#888;margin-bottom:24px;font-size:.9rem}
.grid{display:flex;flex-wrap:wrap;gap:16px;justify-content:center}
.card{background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:20px;width:220px}
.card h2{color:#00ffff;font-size:1rem;margin-bottom:10px}
.card p{font-size:.85rem;color:#aaa;margin-bottom:14px}
a.btn{display:inline-block;background:#0f3460;color:#00ffff;padding:8px 16px;border-radius:4px;text-decoration:none;font-size:.85rem;border:1px solid #00ffff}
a.btn:hover{background:#00ffff;color:#1a1a2e}
.badge{display:inline-block;background:#0a7a0a;color:#7fff7f;padding:3px 10px;border-radius:12px;font-size:.75rem;margin-bottom:18px}
</style>
</head>
<body>
<h1>ElectroShop Dashboard</h1>
<p class="sub"><span class="badge">&#x25cf; Connected to ESP32</span></p>
<div class="grid">
<div class="card">
<h2>Code Editor</h2>
<p>Browse, edit and save files on the device filesystem.</p>
<a class="btn" href="/editor">Open Editor</a>
</div>
<div class="card">
<h2>Files</h2>
<p>View files stored in LittleFS on the ESP32.</p>
<a class="btn" href="/files">List Files</a>
</div>
<div class="card">
<h2>API Status</h2>
<p>Network info, heap usage and filesystem stats.</p>
<a class="btn" href="/api/status">View Status</a>
</div>
</div>
</body>
</html>)rawhtml";

static const char EDITOR_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Editor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:#1a1a2e;color:#e0e0e0;font-family:sans-serif}
#layout{display:flex;height:100vh;flex-direction:column}
#toolbar{background:#0f3460;padding:8px 12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
#toolbar span{color:#00ffff;font-weight:bold;font-size:1rem;margin-right:8px}
#toolbar input{background:#1a1a2e;border:1px solid #00ffff;color:#e0e0e0;padding:4px 8px;border-radius:4px;font-size:.85rem;width:160px}
button{background:#0f3460;color:#00ffff;border:1px solid #00ffff;padding:6px 14px;border-radius:4px;cursor:pointer;font-size:.85rem}
button:hover{background:#00ffff;color:#1a1a2e}
#main{display:flex;flex:1;overflow:hidden}
#sidebar{width:200px;background:#16213e;border-right:1px solid #0f3460;overflow-y:auto;padding:8px}
#sidebar h3{color:#00ffff;font-size:.85rem;margin-bottom:8px;padding-bottom:4px;border-bottom:1px solid #0f3460}
.file-item{padding:6px 8px;border-radius:4px;cursor:pointer;font-size:.8rem;color:#ccc;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.file-item:hover{background:#0f3460;color:#00ffff}
.file-item.active{background:#0f3460;color:#00ffff}
#editor-wrap{flex:1;display:flex;flex-direction:column;overflow:hidden}
#editor{flex:1;width:100%;background:#0d0d1a;color:#e0e0e0;font-family:monospace;font-size:.9rem;padding:12px;border:none;resize:none;outline:none;line-height:1.5}
#statusbar{background:#0f3460;padding:4px 12px;font-size:.75rem;color:#888}
</style>
</head>
<body>
<div id="layout">
<div id="toolbar">
<span>ESP32 Editor</span>
<input id="filename" placeholder="filename.txt" />
<button onclick="saveFile()">Save</button>
<button onclick="newFile()">New</button>
<button onclick="deleteFile()">Delete</button>
<button onclick="loadFiles()">Refresh</button>
<a href="/" style="color:#888;font-size:.8rem;text-decoration:none;margin-left:auto">&#8592; Dashboard</a>
</div>
<div id="main">
<div id="sidebar">
<h3>Files</h3>
<div id="file-list"></div>
</div>
<div id="editor-wrap">
<textarea id="editor" spellcheck="false" placeholder="Select a file or create a new one..."></textarea>
<div id="statusbar" id="statusbar">No file open</div>
</div>
</div>
</div>
<script>
var currentFile = '';
function setStatus(msg){document.getElementById('statusbar').textContent=msg;}
function loadFiles(){
  fetch('/files').then(r=>r.json()).then(files=>{
    var list=document.getElementById('file-list');
    list.innerHTML='';
    files.forEach(function(f){
      var d=document.createElement('div');
      d.className='file-item';
      d.textContent=f;
      d.onclick=function(){loadFile(f);};
      list.appendChild(d);
    });
    if(files.length===0){list.innerHTML='<div style="color:#555;font-size:.75rem">No files</div>';}
  }).catch(function(){setStatus('Error loading files');});
}
function loadFile(name){
  currentFile=name;
  document.getElementById('filename').value=name;
  document.querySelectorAll('.file-item').forEach(function(el){
    el.classList.toggle('active',el.textContent===name);
  });
  fetch('/file?name='+encodeURIComponent(name)).then(function(r){
    if(!r.ok){setStatus('File not found: '+name);return Promise.reject();}
    return r.text();
  }).then(function(t){
    document.getElementById('editor').value=t;
    setStatus('Loaded: '+name+' ('+t.length+' bytes)');
  }).catch(function(){});
}
function saveFile(){
  var name=document.getElementById('filename').value.trim();
  if(!name){alert('Enter a filename first.');return;}
  currentFile=name;
  var body=document.getElementById('editor').value;
  fetch('/file?name='+encodeURIComponent(name),{method:'POST',body:body,headers:{'Content-Type':'text/plain'}})
    .then(function(r){
      if(r.ok){setStatus('Saved: '+name+' ('+body.length+' bytes)');loadFiles();}
      else{setStatus('Save failed');}
    }).catch(function(){setStatus('Save error');});
}
function deleteFile(){
  var name=document.getElementById('filename').value.trim();
  if(!name){alert('No file selected.');return;}
  if(!confirm('Delete '+name+'?')){return;}
  fetch('/file?name='+encodeURIComponent(name),{method:'DELETE'}).then(function(r){
    if(r.ok){document.getElementById('editor').value='';currentFile='';document.getElementById('filename').value='';setStatus('Deleted: '+name);loadFiles();}
    else{setStatus('Delete failed');}
  }).catch(function(){setStatus('Delete error');});
}
function newFile(){
  var name=prompt('New filename:');
  if(!name||!name.trim()){return;}
  currentFile=name.trim();
  document.getElementById('filename').value=currentFile;
  document.getElementById('editor').value='';
  document.querySelectorAll('.file-item').forEach(function(el){el.classList.remove('active');});
  setStatus('New file: '+currentFile+' (unsaved)');
}
loadFiles();
</script>
</body>
</html>)rawhtml";

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

static void handleRoot() {
    webServer.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handleEditor() {
    webServer.send_P(200, "text/html", EDITOR_HTML);
}

static void handleGetFiles() {
    fs::File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory()) {
        webServer.send(500, "application/json", "[]");
        return;
    }
    String json = "[";
    bool first = true;
    fs::File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            if (!first) json += ",";
            json += "\"";
            // strip leading slash from name
            String name = String(entry.name());
            if (name.startsWith("/")) name = name.substring(1);
            json += name;
            json += "\"";
            first = false;
        }
        entry = root.openNextFile();
    }
    json += "]";
    webServer.send(200, "application/json", json);
}

static void handleGetFile() {
    if (!webServer.hasArg("name")) {
        webServer.send(400, "text/plain", "Missing name parameter");
        return;
    }
    String name = webServer.arg("name");
    if (!name.startsWith("/")) name = "/" + name;

    fs::File f = LittleFS.open(name, "r");
    if (!f) {
        webServer.send(404, "text/plain", "File not found: " + name);
        return;
    }
    webServer.streamFile(f, "text/plain");
    f.close();
}

static void handlePostFile() {
    if (!webServer.hasArg("name")) {
        webServer.send(400, "text/plain", "Missing name parameter");
        return;
    }
    String name = webServer.arg("name");
    if (!name.startsWith("/")) name = "/" + name;

    String body = webServer.arg("plain");
    fs::File f = LittleFS.open(name, "w");
    if (!f) {
        webServer.send(500, "text/plain", "Failed to open file for writing");
        return;
    }
    f.print(body);
    f.close();
    webServer.send(200, "text/plain", "OK");
}

static void handleDeleteFile() {
    if (!webServer.hasArg("name")) {
        webServer.send(400, "text/plain", "Missing name parameter");
        return;
    }
    String name = webServer.arg("name");
    if (!name.startsWith("/")) name = "/" + name;

    if (LittleFS.exists(name)) {
        LittleFS.remove(name);
        webServer.send(200, "text/plain", "OK");
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

static void handleFileRequest() {
    String method = webServer.method() == HTTP_GET    ? "GET"
                  : webServer.method() == HTTP_POST   ? "POST"
                  : webServer.method() == HTTP_DELETE ? "DELETE"
                  : "OTHER";

    if (webServer.method() == HTTP_GET)    { handleGetFile();    return; }
    if (webServer.method() == HTTP_POST)   { handlePostFile();   return; }
    if (webServer.method() == HTTP_DELETE) { handleDeleteFile(); return; }

    webServer.send(405, "text/plain", "Method Not Allowed");
}

static void handleApiStatus() {
    String ip = WiFi.localIP().toString();
    String ssid = WiFi.SSID();

    String json = "{";
    json += "\"ssid\":\"" + ssid + "\",";
    json += "\"ip\":\"" + ip + "\",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"littlefs_used\":" + String(LittleFS.usedBytes()) + ",";
    json += "\"littlefs_total\":" + String(LittleFS.totalBytes());
    json += "}";

    webServer.send(200, "application/json", json);
}

static void handleNotFound() {
    webServer.send(404, "text/plain", "Not Found");
}

// ---------------------------------------------------------------------------
// Lifecycle functions
// ---------------------------------------------------------------------------

void initWebServer() {
    if (!LittleFS.begin(true)) {
        Serial.println("[WebServer] LittleFS mount failed — continuing anyway");
    } else {
        Serial.println("[WebServer] LittleFS mounted OK");
    }

    webServer.on("/",           HTTP_GET,    handleRoot);
    webServer.on("/editor",     HTTP_GET,    handleEditor);
    webServer.on("/files",      HTTP_GET,    handleGetFiles);
    webServer.on("/file",                    handleFileRequest);  // GET/POST/DELETE
    webServer.on("/api/status", HTTP_GET,    handleApiStatus);
    webServer.onNotFound(handleNotFound);

    Serial.println("[WebServer] Routes registered");
}

void startWebServer() {
    webServer.begin();
    webServerRunning = true;
    Serial.println("[WebServer] Started on port 80");
}

void stopWebServer() {
    webServer.stop();
    webServerRunning = false;
    Serial.println("[WebServer] Stopped");
}

void handleWebServer() {
    if (webServerRunning) {
        webServer.handleClient();
    }
}
