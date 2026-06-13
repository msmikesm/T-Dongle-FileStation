#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <LGFX_Setup.h>

// ============================================
// SD CARD - HSPI (SPI3) - osobny bus
// ============================================
#define SD_CS 47
#define SD_MOSI 18
#define SD_MISO 16
#define SD_SCK 17

// Display ma swoje piny w LGFX_Setup.h (SCK=10, MOSI=11, DC=13, CS=12, RST=14)
// To jest na VSPI (SPI2) - domyślnie

// WiFi Configuration
const char *ap_ssid = "TDongle-FileStation";
const char *ap_password = "12345678";

// Web server
AsyncWebServer server(80);

// Display
LGFX display;

// SD card info
uint64_t cardSize = 0;
uint64_t cardFree = 0;
int fileCount = 0;

// DNS Server
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Osobny SPI dla SD
SPIClass sdSpi(HSPI); // HSPI = SPI3

// ============================================
// HTML Web Interface (ten sam)
// ============================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>T-Dongle FileStation</title>
    <style>
        body { font-family: Arial; max-width: 800px; margin: 0 auto; padding: 20px; background: #f5f5f5; }
        h1 { color: #2c3e50; text-align: center; }
        .upload-form { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        input[type="file"] { margin: 10px 0; padding: 10px; }
        button { background: #3498db; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
        button:hover { background: #2980b9; }
        .file-list { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .file-item { padding: 10px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }
        .file-name { text-decoration: none; color: #2c3e50; flex-grow: 1; text-overflow: ellipsis; }
        .delete-btn { background: #e74c3c; color: white; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; font-size: 12px; }
        .delete-btn:hover { background: #c0392b; }
        .info { background: #d4edda; padding: 10px; border-radius: 5px; margin-bottom: 20px; text-align: center; }
        hr { margin: 20px 0; }
    </style>
</head>
<body>
    <h1>📁 T-Dongle FileStation</h1>
    <div class="info" id="info">Loading...</div>
    
    <div class="upload-form">
        <h3>📤 Upload File</h3>
        <form action="/upload" method="POST" enctype="multipart/form-data" id="upload-form">
            <input type="file" name="file" id="file-input" required>
            <button type="submit">Upload</button>
        </form>
        <div id="upload-status"></div>
    </div>
    
    <div class="file-list">
        <h3>📄 Files on SD Card</h3>
        <div id="file-list">Loading...</div>
    </div>
    
    <hr>
    <p style="text-align: center; color: #7f8c8d; font-size: 12px;">ESP32-S3 T-Dongle FileStation</p>
    
    <script>
        function loadFiles() {
            fetch('/api/files')
                .then(response => response.json())
                .then(data => {
                    const infoDiv = document.getElementById('info');
                    infoDiv.innerHTML = '💾 SD Card: ' + data.used + ' / ' + data.total + ' MB | 📄 Files: ' + data.count;
                    
                    const fileList = document.getElementById('file-list');
                    if (data.files.length === 0) {
                        fileList.innerHTML = '<p style="color: #7f8c8d;">No files. Upload something above!</p>';
                        return;
                    }
                    
                    let html = '';
                    for (let i = 0; i < data.files.length; i++) {
                        let file = data.files[i];
                        html += '<div class="file-item">' +
                            '<a href="/download/' + encodeURIComponent(file.name) + '" class="file-name">📄 ' + file.name + ' (' + file.size + ' KB)</a>' +
                            '<button class="delete-btn" onclick="deleteFile(\'' + file.name.replace(/'/g, "\\'") + '\')">🗑️ Delete</button>' +
                            '</div>';
                    }
                    fileList.innerHTML = html;
                });
        }
        
        function deleteFile(filename) {
            if (confirm('Delete ' + filename + '?')) {
                fetch('/api/delete/' + encodeURIComponent(filename), { method: 'DELETE' })
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            loadFiles();
                        } else {
                            alert('Delete failed: ' + data.error);
                        }
                    });
            }
        }
        
        document.getElementById('upload-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const statusDiv = document.getElementById('upload-status');
            statusDiv.innerHTML = '<p>Uploading...</p>';
            
            fetch('/upload', { method: 'POST', body: formData })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        statusDiv.innerHTML = '<p style="color: green;">✅ ' + data.message + '</p>';
                        document.getElementById('file-input').value = '';
                        loadFiles();
                        setTimeout(function() { statusDiv.innerHTML = ''; }, 3000);
                    } else {
                        statusDiv.innerHTML = '<p style="color: red;">❌ Error: ' + data.error + '</p>';
                    }
                });
        });
        
        loadFiles();
        setInterval(loadFiles, 5000);
    </script>
</body>
</html>
)rawliteral";

// ============================================
// Update Display
// ============================================
void updateDisplay()
{
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("T-Dongle FileStation");
  display.println("");
  display.print("WiFi: ");
  display.println(ap_ssid);
  display.print("IP: 192.168.4.1");
  display.println("");

  if (cardSize > 0)
  {
    display.print("SD: ");
    display.print((cardSize - cardFree) / 1048576);
    display.print("/");
    display.print(cardSize / 1048576);
    display.println(" MB");
    display.print("Files: ");
    display.println(fileCount);
  }
  else
  {
    display.println("SD: NO CARD!");
  }

  display.println("");
  display.println("Open in browser:");
  display.println("192.168.4.1 | http://some.local");
}

// ============================================
// Scan SD Card
// ============================================
void scanFiles()
{
  File root = SD.open("/");
  if (!root)
  {
    fileCount = 0;
    return;
  }

  fileCount = 0;
  cardFree = SD.totalBytes() - SD.usedBytes();
  cardSize = SD.totalBytes();

  File file = root.openNextFile();
  while (file)
  {
    if (!file.isDirectory())
    {
      fileCount++;
    }
    file = root.openNextFile();
  }
  root.close();
}

// ============================================
// Initialize SD Card - HSPI (osobny SPI)
// ============================================
bool initSDCard()
{
  Serial.println("\n========================================");
  Serial.println("Initializing SD Card (HSPI - separate bus)");
  Serial.println("========================================");
  Serial.printf("Pins:\n");
  Serial.printf("  CS   = GPIO%d\n", SD_CS);
  Serial.printf("  MOSI = GPIO%d\n", SD_MOSI);
  Serial.printf("  MISO = GPIO%d\n", SD_MISO);
  Serial.printf("  SCK  = GPIO%d\n", SD_SCK);

  // Initialize dedicated SPI for SD card
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Try mounting at safe speed
  Serial.println("Mounting SD card at 400kHz...");
  bool mounted = SD.begin(SD_CS, sdSpi, 400000);

  if (!mounted)
  {
    Serial.println("Failed at 400kHz, trying 1MHz...");
    mounted = SD.begin(SD_CS, sdSpi, 1000000);
  }

  if (!mounted)
  {
    Serial.println("Failed at 1MHz, trying 4MHz...");
    mounted = SD.begin(SD_CS, sdSpi, 4000000);
  }

  if (!mounted)
  {
    Serial.println("\n❌ SD Card initialization FAILED!");
    return false;
  }

  // Success
  cardSize = SD.totalBytes();
  cardFree = cardSize - SD.usedBytes();

  Serial.println("\n✅✅✅ SD CARD MOUNTED SUCCESSFULLY! ✅✅✅");
  Serial.printf("  Total size: %llu MB\n", cardSize / 1048576);
  Serial.printf("  Free space: %llu MB\n", cardFree / 1048576);

  return true;
}

// ============================================
// Initialize Display - VSPI (domyślny SPI)
// ============================================
bool initDisplay()
{
  Serial.println("\nInitializing Display (VSPI - default bus)...");

  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);

  Serial.println("Display ready!");
  return true;
}

// ============================================
// Setup API Endpoints
// ============================================
void setupFileAPI()
{
  // API: Get file list
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        String json = "{\"total\":" + String(cardSize / 1048576) + 
                      ",\"used\":" + String((cardSize - cardFree) / 1048576) +
                      ",\"count\":" + String(fileCount) + ",\"files\":[";
        
        File root = SD.open("/");
        File file = root.openNextFile();
        bool first = true;
        while (file) {
            if (!file.isDirectory()) {
                if (!first) json += ",";
                json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size() / 1024) + "}";
                first = false;
            }
            file = root.openNextFile();
        }
        root.close();
        json += "]}";
        request->send(200, "application/json", json); });

  // API: Upload file
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", "{\"success\":true,\"message\":\"File uploaded\"}"); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
        static File uploadFile;
        
        if (!index) {
            String path = "/" + filename;
            uploadFile = SD.open(path, FILE_WRITE);
            if (!uploadFile) {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"Cannot create file\"}");
                return;
            }
        }
        
        if (uploadFile && len > 0) {
            uploadFile.write(data, len);
        }
        
        if (final && uploadFile) {
            uploadFile.close();
            scanFiles();
            updateDisplay();
        } });

  // Handle dynamic routes
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
        String url = request->url();
        String method = request->methodToString();
        
        if (url.startsWith("/api/delete/") && method == "DELETE") {
            String filename = url.substring(12);
            String path = "/" + filename;
            
            if (SD.remove(path)) {
                scanFiles();
                updateDisplay();
                request->send(200, "application/json", "{\"success\":true,\"message\":\"File deleted\"}");
            } else {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"Delete failed\"}");
            }
        }
        else if (url.startsWith("/download/") && method == "GET") {
            String filename = url.substring(9);
            filename.replace("%20", " ");
            String path = "/" + filename;
            
            if (SD.exists(path)) {
                request->send(SD, path, "application/octet-stream");
            } else {
                request->send(404, "text/plain", "File not found: " + filename);
            }
        }
        else if (url == "/" && method == "GET") {
            request->send(200, "text/html", index_html);
        }
        else {
            request->send(404, "text/plain", "Not found");
        } });

  server.begin();
  Serial.println("HTTP server started");
}

// ============================================
// Setup
// ============================================
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== T-Dongle FileStation ===");

  // Step 1: Initialize SD card (HSPI - osobny SPI)
  bool sdOK = initSDCard();

  // Step 2: Initialize display (VSPI - domyślny SPI, NIE koliduje!)
  bool displayOK = initDisplay();

  if (sdOK && displayOK)
  {
    Serial.println("\n✅ Both SD card and Display are working on separate SPI buses!");
  }
  else if (sdOK)
  {
    Serial.println("\n⚠️ SD card OK, but display has issues");
  }
  else
  {
    Serial.println("\n❌ SD card failed - check connections");
  }

  // Start WiFi AP
  Serial.println("\nStarting WiFi AP...");
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password, 6, 0, 6);

  // Start DNS server
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS started - any domain works");

  IPAddress IP = WiFi.softAPIP();

  // Update display
  if (displayOK)
  {
    updateDisplay();
  }

  // Start web server
  setupFileAPI();

  Serial.println("\n========================================");
  Serial.println("Setup Complete!");
  Serial.printf("WiFi AP: %s (password: %s)\n", ap_ssid, ap_password);
  Serial.printf("Open http://%s in your browser\n", IP.toString().c_str());
  Serial.println("========================================\n");
}

// ============================================
// Loop
// ============================================
void loop()
{
  dnsServer.processNextRequest();

  static unsigned long lastScan = 0;

  if (millis() - lastScan > 10000 && cardSize > 0)
  {
    scanFiles();
    updateDisplay();
    lastScan = millis();
  }

  delay(100);
}
