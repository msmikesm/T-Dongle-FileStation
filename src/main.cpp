#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LGFX_Setup.h>

// ============================================
// SD CARD PINS
// ============================================
#define SD_CS 47
#define SD_MOSI 18
#define SD_MISO 16
#define SD_SCK 17

// WiFi
const char *ap_ssid = "TDongle-FileStation";
const char *ap_password = "12345678";

// Server
AsyncWebServer server(80);

// Display
LGFX display;

// SD status
bool sdOk = false;

// Operation lock - only ONE operation at a time!
volatile bool sdBusy = false;
SPIClass sdSpi(HSPI);

// ============================================
// HTML - manual refresh only
// ============================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>T-Dongle FileStation</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: Arial; max-width: 1000px; margin: 0 auto; padding: 20px; background: #f5f5f5; }
        h1 { color: #2c3e50; text-align: center; }
        .card { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .upload-area { border: 2px dashed #3498db; border-radius: 10px; padding: 30px; text-align: center; cursor: pointer; }
        .upload-area.drag-over { background: #e3f2fd; }
        .btn { background: #3498db; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 5px; }
        .btn:hover { background: #2980b9; }
        .btn-refresh { background: #27ae60; }
        .btn-refresh:hover { background: #219a52; }
        .queue-item { background: #f8f9fa; margin: 10px 0; padding: 10px; border-radius: 5px; }
        .progress-bar { background: #e0e0e0; border-radius: 5px; height: 20px; overflow: hidden; margin-top: 5px; }
        .progress-fill { background: #4caf50; height: 100%; width: 0%; }
        .file-item { padding: 10px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }
        .file-name { color: #2c3e50; flex-grow: 1; text-decoration: none; }
        .delete-btn { background: #e74c3c; color: white; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; }
        .info { background: #d4edda; padding: 10px; border-radius: 5px; margin-bottom: 20px; text-align: center; }
        .info-busy { background: #f39c12; color: white; }
        .info-error { background: #e74c3c; color: white; }
        .badge { background: #3498db; color: white; padding: 2px 8px; border-radius: 10px; font-size: 12px; margin-left: 10px; }
    </style>
</head>
<body>
    <h1>📁 T-Dongle FileStation</h1>
    <div class="info" id="info">Loading...</div>
    
    <div class="card">
        <h3>📤 Upload Files</h3>
        <div class="upload-area" id="dropZone">
            <p>📂 Drag & drop or click to select</p>
            <button class="btn" id="selectBtn">Select Files</button>
            <input type="file" id="fileInput" multiple style="display:none">
        </div>
        <div id="queueContainer"></div>
    </div>
    
    <div class="card">
        <h3 style="display: inline-block;">📄 Files</h3>
        <span class="badge" id="fileCountBadge">0</span>
        <button class="btn btn-refresh" id="refreshBtn" style="float: right;">🔄 Refresh</button>
        <div style="clear: both;"></div>
        <div id="fileList">Loading...</div>
    </div>
    
    <script>
        let queue = [];
        let uploading = false;
        
        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');
        const selectBtn = document.getElementById('selectBtn');
        const refreshBtn = document.getElementById('refreshBtn');
        
        dropZone.onclick = () => fileInput.click();
        dropZone.ondragover = (e) => { e.preventDefault(); dropZone.classList.add('drag-over'); };
        dropZone.ondragleave = () => dropZone.classList.remove('drag-over');
        dropZone.ondrop = (e) => {
            e.preventDefault();
            dropZone.classList.remove('drag-over');
            addFiles(e.dataTransfer.files);
        };
        fileInput.onchange = (e) => addFiles(e.target.files);
        selectBtn.onclick = (e) => { e.stopPropagation(); fileInput.click(); };
        refreshBtn.onclick = () => loadFiles();
        
        function addFiles(files) {
            for (let f of files) {
                queue.push({ id: Date.now() + Math.random(), file: f, progress: 0, status: 'pending' });
            }
            renderQueue();
            processQueue();
        }
        
        function renderQueue() {
            let html = '<h4>Queue:</h4>';
            for (let item of queue) {
                let status = item.status === 'uploading' ? '⏫ Uploading...' : (item.status === 'success' ? '✅ Done' : (item.status === 'error' ? '❌ Failed' : '⏳ Waiting'));
                html += `<div class="queue-item">
                    <div>📄 ${item.file.name} (${(item.file.size/1024).toFixed(1)} KB) - ${status}</div>
                    <div class="progress-bar"><div class="progress-fill" style="width: ${item.progress}%"></div></div>
                </div>`;
            }
            document.getElementById('queueContainer').innerHTML = html || '';
        }
        
        async function processQueue() {
            if (uploading) return;
            let next = queue.find(i => i.status === 'pending');
            if (!next) return;
            
            uploading = true;
            next.status = 'uploading';
            renderQueue();
            
            try {
                let formData = new FormData();
                formData.append('file', next.file);
                
                let xhr = new XMLHttpRequest();
                xhr.open('POST', '/upload');
                xhr.upload.onprogress = (e) => { if (e.lengthComputable) { next.progress = (e.loaded / e.total) * 100; renderQueue(); } };
                
                await new Promise((resolve, reject) => {
                    xhr.onload = () => xhr.status === 200 ? resolve() : reject();
                    xhr.onerror = () => reject();
                    xhr.send(formData);
                });
                
                next.status = 'success';
                next.progress = 100;
                renderQueue();
            } catch(e) {
                next.status = 'error';
                renderQueue();
            }
            
            uploading = false;
            processQueue();
        }
        
        async function loadFiles() {
            let infoDiv = document.getElementById('info');
            infoDiv.innerHTML = '⏳ Loading...';
            infoDiv.className = 'info';
            
            try {
                let res = await fetch('/api/files');
                let data = await res.json();
                
                if (data.error) {
                    infoDiv.innerHTML = `❌ ${data.error}`;
                    infoDiv.className = 'info info-error';
                    return;
                }
                
                infoDiv.innerHTML = `💾 SD: ${data.used} / ${data.total} MB | Files: ${data.count}`;
                document.getElementById('fileCountBadge').innerText = data.count;
                
                if (!data.files || data.files.length === 0) {
                    document.getElementById('fileList').innerHTML = '<p>No files</p>';
                    return;
                }
                
                let html = '';
                for (let f of data.files) {
                    html += `<div class="file-item">
                        <a href="/download/${encodeURIComponent(f.name)}" class="file-name" target="_blank">📄 ${f.name} (${f.size} KB)</a>
                        <button class="delete-btn" onclick="deleteFile('${f.name.replace(/'/g, "\\'")}')">Delete</button>
                    </div>`;
                }
                document.getElementById('fileList').innerHTML = html;
            } catch(e) {
                infoDiv.innerHTML = '❌ Connection error';
                infoDiv.className = 'info info-error';
            }
        }
        
        async function deleteFile(name) {
            if (!confirm('Delete?')) return;
            
            let infoDiv = document.getElementById('info');
            infoDiv.innerHTML = '⏳ Deleting...';
            
            try {
                let res = await fetch('/api/delete/' + encodeURIComponent(name), { method: 'DELETE' });
                let data = await res.json();
                if (data.success) {
                    loadFiles();
                } else {
                    infoDiv.innerHTML = `❌ ${data.error}`;
                    infoDiv.className = 'info info-error';
                }
            } catch(e) {
                infoDiv.innerHTML = '❌ Delete failed';
                infoDiv.className = 'info info-error';
            }
        }
        
        // Load files ONCE on page load
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

// ============================================
// Display - ONLY at startup
// ============================================
void showStartupStatus()
{
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("T-Dongle FileStation");
  display.println("");
  display.print("WiFi: ");
  display.println(ap_ssid);
  display.print("IP: ");
  display.println(WiFi.softAPIP().toString());
  display.println("");

  if (sdOk)
  {
    display.println("SD Card: READY");
    display.print("Size: ");
    display.print(SD.totalBytes() / 1048576);
    display.println(" MB");
  }
  else
  {
    display.println("SD Card: ERROR");
  }
}

// ============================================
// SD Card init
// ============================================
bool initSD()
{
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSpi, 4000000))
  {
    if (!SD.begin(SD_CS, sdSpi, 1000000))
    {
      if (!SD.begin(SD_CS, sdSpi, 400000))
      {
        return false;
      }
    }
  }
  return true;
}

// ============================================
// Get file list safely
// ============================================
String getFileList()
{
  if (sdBusy)
  {
    return "{\"error\":\"SD card busy, try again later\"}";
  }

  sdBusy = true;

  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();

  String json = "{\"total\":" + String(total / 1048576) +
                ",\"used\":" + String(used / 1048576) +
                ",\"count\":0,\"files\":[";

  File root = SD.open("/");
  if (root)
  {
    File f = root.openNextFile();
    bool first = true;
    int cnt = 0;
    while (f)
    {
      if (!f.isDirectory())
      {
        if (!first)
          json += ",";
        String name = String(f.name());
        name.replace("\"", "\\\"");
        json += "{\"name\":\"" + name + "\",\"size\":" + String(f.size() / 1024) + "}";
        first = false;
        cnt++;
      }
      f = root.openNextFile();
    }
    root.close();
    json.replace("\"count\":0", "\"count\":" + String(cnt));
  }
  json += "]}";

  sdBusy = false;
  return json;
}

// ============================================
// Setup
// ============================================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // SD card
  sdOk = initSD();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  // Display startup status (ONCE)
  showStartupStatus();

  // ============================================
  // API - all operations respect sdBusy lock
  // ============================================

  // Get file list
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", getFileList()); });

  // Upload file
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", "{\"success\":true}"); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
      static File f;
      
      if (!index) {
        // Wait if busy
        while (sdBusy) delay(10);
        sdBusy = true;
        
        String path = "/" + filename;
        f = SD.open(path, FILE_WRITE);
        if (!f) {
          sdBusy = false;
          return;
        }
      }
      
      if (f && len > 0) {
        f.write(data, len);
      }
      
      if (final && f) {
        f.close();
        sdBusy = false;
      } });

  // Delete file
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    String url = request->url();
    String method = request->methodToString();
    
    if (url.startsWith("/api/delete/") && method == "DELETE") {
      // Wait if busy
      while (sdBusy) delay(10);
      sdBusy = true;
      
      String filename = url.substring(12);
      String path = "/" + filename;
      
      bool success = SD.remove(path);
      sdBusy = false;
      
      if (success) {
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Delete failed\"}");
      }
    }
    else if (url.startsWith("/download/") && method == "GET") {
      // Wait if busy
      while (sdBusy) delay(10);
      sdBusy = true;
      
      String filename = url.substring(9);
      filename.replace("%20", " ");
      String path = "/" + filename;
      
      if (SD.exists(path)) {
        String contentType = "application/octet-stream";
        if (filename.endsWith(".txt") || filename.endsWith(".html") || filename.endsWith(".css") || filename.endsWith(".js")) {
          contentType = "text/plain";
        } else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) {
          contentType = "image/jpeg";
        } else if (filename.endsWith(".png")) {
          contentType = "image/png";
        } else if (filename.endsWith(".pdf")) {
          contentType = "application/pdf";
        }
        
        request->send(SD, path, contentType);
        sdBusy = false;
      } else {
        sdBusy = false;
        request->send(404, "text/plain", "File not found");
      }
    }
    else if (url == "/" && method == "GET") {
      request->send(200, "text/html", index_html);
    }
    else {
      request->send(404);
    } });

  server.begin();
  Serial.printf("Server ready: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void loop()
{
  delay(100);
}
