#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#define DISPLAY_HEIGHT 64
#define DISPLAY_WIDTH 128

Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> eyes(display);

uint8_t sensor = 4;
byte err, address;
byte my_add = 0;
uint16_t REFRESH_RATE = 100;

Preferences preferences;
String wifiSSID = "";
String wifiPassword = "";
bool shouldRestart = false;
uint32_t restartTimer = 0;

WebServer server(80);

enum ScreenState {
    STATE_FACE,
    STATE_MOISTURE
};

enum MOOD {
    normal,
    angry,
    happy,
    confused
};

uint32_t faceDuration = 5000;
uint32_t moistureDuration = 2000;
bool autoMood = true;
uint8_t manualMood = normal;

bool shouldUpdate = false;
String otaUrl = "";

const char dashboardHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RoboPot Dashboard</title>
    <style>
        body {
            font-family: system-ui, -apple-system, sans-serif;
            background-color: #f9f9f9;
            color: #111111;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 100%;
            max-width: 600px;
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
            padding: 24px;
            margin: 16px;
        }
        h1 {
            font-size: 24px;
            font-weight: 700;
            margin-top: 0;
            margin-bottom: 24px;
            text-align: center;
            border-bottom: 2px solid #111111;
            padding-bottom: 12px;
        }
        .tabs {
            display: flex;
            border-bottom: 1px solid #e2e8f0;
            margin-bottom: 24px;
        }
        .tab-btn {
            flex: 1;
            padding: 12px;
            background: none;
            border: none;
            font-weight: 600;
            color: #718096;
            cursor: pointer;
            text-align: center;
            font-size: 14px;
        }
        .tab-btn.active {
            color: #111111;
            border-bottom: 2px solid #111111;
        }
        .tab-content {
            display: none;
        }
        .tab-content.active {
            display: block;
        }
        .card {
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 16px;
            margin-bottom: 16px;
        }
        .card-title {
            font-weight: 700;
            font-size: 16px;
            margin-bottom: 12px;
        }
        .moisture-val {
            font-size: 48px;
            font-weight: 800;
            text-align: center;
            margin: 16px 0;
        }
        .progress-container {
            background-color: #edf2f7;
            border-radius: 9999px;
            height: 12px;
            overflow: hidden;
            margin-bottom: 16px;
        }
        .progress-bar {
            background-color: #111111;
            height: 100%;
            width: 0%;
            transition: width 0.5s ease;
        }
        .status-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        .status-item {
            background-color: #f7fafc;
            padding: 12px;
            border-radius: 6px;
            font-size: 13px;
        }
        .status-label {
            color: #718096;
            margin-bottom: 4px;
        }
        .status-value {
            font-weight: 700;
        }
        .form-group {
            margin-bottom: 16px;
        }
        label {
            display: block;
            font-weight: 600;
            font-size: 14px;
            margin-bottom: 6px;
        }
        select, input {
            width: 100%;
            padding: 10px;
            border: 1px solid #cbd5e0;
            border-radius: 6px;
            font-size: 14px;
            box-sizing: border-box;
            background-color: #ffffff;
            color: #111111;
        }
        button.btn {
            width: 100%;
            padding: 12px;
            background-color: #111111;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            font-weight: 700;
            font-size: 14px;
            cursor: pointer;
            transition: background 0.2s;
        }
        button.btn:hover {
            background-color: #2d3748;
        }
        .guide-text {
            font-size: 14px;
            line-height: 1.6;
            color: #2d3748;
        }
        .guide-text h3 {
            margin-top: 16px;
            margin-bottom: 8px;
            font-size: 15px;
            color: #111111;
            border-bottom: 1px solid #edf2f7;
            padding-bottom: 4px;
        }
        .guide-text ul {
            padding-left: 20px;
            margin: 8px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>RoboPot Dashboard</h1>
        <div class="tabs">
            <button class="tab-btn active" onclick="openTab('dash')">Monitor</button>
            <button class="tab-btn" onclick="openTab('config')">Kontrol</button>
            <button class="tab-btn" onclick="openTab('guide')">Panduan</button>
            <button class="tab-btn" onclick="openTab('ota')">Update</button>
            <button class="tab-btn" onclick="window.location.href='/config-wifi'">Wi-Fi</button>
        </div>

        <div id="dash" class="tab-content active">
            <div class="card">
                <div class="card-title" style="text-align: center;">Kelembapan Tanah</div>
                <div class="moisture-val"><span id="moist-val">0</span>%</div>
                <div class="progress-container">
                    <div id="moist-bar" class="progress-bar"></div>
                </div>
            </div>
            <div class="status-grid">
                <div class="status-item">
                    <div class="status-label">Mood Aktif</div>
                    <div id="status-mood" class="status-value">-</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Mode Mood</div>
                    <div id="status-mood-mode" class="status-value">-</div>
                </div>
                <div class="status-item">
                    <div class="status-label">IP Address</div>
                    <div id="status-ip" class="status-value">-</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Sinyal Wi-Fi</div>
                    <div id="status-wifi" class="status-value">-</div>
                </div>
            </div>
        </div>

        <div id="config" class="tab-content">
            <div class="card">
                <div class="card-title">Pengaturan Wajah & Mood</div>
                <div class="form-group">
                    <label>Mode Mood</label>
                    <select id="cfg-mood-mode" onchange="toggleMoodSelect()">
                        <option value="1">Otomatis (Acak)</option>
                        <option value="0">Manual</option>
                    </select>
                </div>
                <div class="form-group" id="cfg-mood-group" style="display: none;">
                    <label>Pilih Mood Manual</label>
                    <select id="cfg-mood">
                        <option value="0">Normal</option>
                        <option value="1">Angry</option>
                        <option value="2">Happy</option>
                        <option value="3">Confused</option>
                    </select>
                </div>
            </div>
            <div class="card">
                <div class="card-title">Durasi Tampilan Layar</div>
                <div class="form-group">
                    <label>Durasi Wajah (Detik)</label>
                    <input type="number" id="cfg-face-dur" min="1" max="60">
                </div>
                <div class="form-group">
                    <label>Durasi Sensor (Detik)</label>
                    <input type="number" id="cfg-sensor-dur" min="1" max="60">
                </div>
            </div>
            <button class="btn" onclick="saveConfig()">Simpan Konfigurasi</button>
        </div>

        <div id="guide" class="tab-content">
            <div class="card guide-text">
                <div class="card-title">Panduan Penggunaan RoboPot</div>
                <h3>Cara Kerja Sistem</h3>
                RoboPot secara otomatis bergantian menampilkan ekspresi wajah robot yang ekspresif dan persentase kelembapan tanah tanaman Anda pada layar OLED SSD1306 bawaan.
                <h3>Panduan Sensor Kelembapan</h3>
                <ul>
                    <li>Tancapkan ujung probe besi sensor kelembapan tanah ke dalam tanah pot tanaman sedalam 3-5 cm.</li>
                    <li>Hindari membasahi bagian sirkuit elektronik kepala sensor saat menyiram tanaman.</li>
                    <li>Jika kelembapan tanah berada di bawah 30%, pot tanaman Anda membutuhkan penyiraman segera.</li>
                </ul>
                <h3>Konfigurasi Koneksi</h3>
                <ul>
                    <li>Anda dapat membuka halaman dasbor ini menggunakan alamat URL <a href="http://robopot.local" style="color: #111111; font-weight: 700;">http://robopot.local</a> saat terhubung di jaringan yang sama.</li>
                    <li>Jika berada di luar jangkauan Wi-Fi rumah, ESP32 akan menyalakan hotspot mandiri bernama <strong>RoboPot-AP</strong> (password: <strong>12345678</strong>).</li>
                </ul>
            </div>
        </div>

        <div id="ota" class="tab-content">
            <div class="card">
                <div class="card-title">Pembaruan Firmware OTA GitHub</div>
                <div style="font-size: 13px; color: #718096; margin-bottom: 16px; line-height: 1.5;">
                    Masukkan URL tautan file biner mentah (.bin) dari repositori GitHub Anda untuk memperbarui program ESP32 ini secara langsung melalui internet.
                </div>
                <div class="form-group">
                    <label>URL Firmware (.bin)</label>
                    <input type="text" id="ota-url" placeholder="https://raw.githubusercontent.com/.../firmware.bin">
                </div>
                <button class="btn" id="ota-btn" onclick="triggerUpdate()">Mulai Update Firmware</button>
                <div id="ota-status" style="margin-top: 16px; font-weight: 700; font-size: 14px; text-align: center; display: none;"></div>
            </div>
        </div>
    </div>

    <script>
        function openTab(tabId) {
            document.querySelectorAll('.tab-content').forEach(tab => tab.classList.remove('active'));
            document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
            document.getElementById(tabId).classList.add('active');
            event.currentTarget.classList.add('active');
        }

        function toggleMoodSelect() {
            const mode = document.getElementById('cfg-mood-mode').value;
            document.getElementById('cfg-mood-group').style.display = (mode === '0') ? 'block' : 'none';
        }

        function loadData() {
            fetch('/api/data')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('moist-val').innerText = data.moisture;
                    document.getElementById('moist-bar').style.width = data.moisture + '%';
                    
                    const moods = ['Normal', 'Angry', 'Happy', 'Confused'];
                    document.getElementById('status-mood').innerText = moods[data.mood] || 'Unknown';
                    document.getElementById('status-mood-mode').innerText = data.autoMood ? 'Otomatis' : 'Manual';
                    document.getElementById('status-ip').innerText = data.ip;
                    document.getElementById('status-wifi').innerText = data.wifi + ' dBm';

                    if (!document.getElementById('config').classList.contains('active')) {
                        document.getElementById('cfg-mood-mode').value = data.autoMood ? "1" : "0";
                        document.getElementById('cfg-mood').value = data.mood;
                        document.getElementById('cfg-face-dur').value = data.faceDur / 1000;
                        document.getElementById('cfg-sensor-dur').value = data.sensorDur / 1000;
                        toggleMoodSelect();
                    }
                })
                .catch(err => console.error(err));
        }

        function saveConfig() {
            const autoMood = document.getElementById('cfg-mood-mode').value;
            const mood = document.getElementById('cfg-mood').value;
            const faceDur = document.getElementById('cfg-face-dur').value * 1000;
            const sensorDur = document.getElementById('cfg-sensor-dur').value * 1000;

            fetch(`/api/set?auto=${autoMood}&mood=${mood}&face=${faceDur}&sensor=${sensorDur}`)
                .then(res => res.text())
                .then(msg => {
                    alert(msg);
                    loadData();
                })
                .catch(err => alert('Gagal menyimpan konfigurasi: ' + err));
        }

        function triggerUpdate() {
            const url = document.getElementById('ota-url').value;
            if (!url) {
                alert('Silakan isi URL file biner terlebih dahulu!');
                return;
            }

            if (!confirm('Apakah Anda yakin ingin memulai pembaruan firmware? ESP32 akan merestart secara otomatis setelah pembaruan selesai.')) {
                return;
            }

            const btn = document.getElementById('ota-btn');
            const status = document.getElementById('ota-status');
            
            btn.disabled = true;
            btn.innerText = 'Mengunduh & Memperbarui...';
            status.style.display = 'block';
            status.style.color = '#111111';
            status.innerText = 'Menghubungi server OTA...';

            fetch(`/api/update?url=${encodeURIComponent(url)}`)
                .then(res => res.text())
                .then(msg => {
                    status.innerText = msg;
                    if (msg.includes('berhasil') || msg.includes('OK')) {
                        status.style.color = '#22543d';
                        status.innerText = 'Update Berhasil! Perangkat sedang merestart. Silakan buka kembali halaman ini dalam 10 detik.';
                    } else {
                        status.style.color = '#742a2a';
                        btn.disabled = false;
                        btn.innerText = 'Mulai Update Firmware';
                    }
                })
                .catch(err => {
                    status.style.color = '#742a2a';
                    status.innerText = 'Gagal memicu pembaruan: ' + err;
                    btn.disabled = false;
                    btn.innerText = 'Mulai Update Firmware';
                });
        }

        loadData();
        setInterval(loadData, 2000);
    </script>
</body>
</html>
)rawliteral";

const char wifiConfigHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Konfigurasi Wi-Fi RoboPot</title>
    <style>
        body {
            font-family: system-ui, -apple-system, sans-serif;
            background-color: #f9f9f9;
            color: #111111;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 100%;
            max-width: 400px;
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
            padding: 24px;
            margin: 16px;
        }
        h1 {
            font-size: 20px;
            font-weight: 700;
            margin-top: 0;
            margin-bottom: 24px;
            text-align: center;
            border-bottom: 2px solid #111111;
            padding-bottom: 12px;
        }
        .form-group {
            margin-bottom: 16px;
        }
        label {
            display: block;
            font-weight: 600;
            font-size: 14px;
            margin-bottom: 6px;
        }
        input {
            width: 100%;
            padding: 10px;
            border: 1px solid #cbd5e0;
            border-radius: 6px;
            font-size: 14px;
            box-sizing: border-box;
            background-color: #ffffff;
            color: #111111;
        }
        button.btn {
            width: 100%;
            padding: 12px;
            background-color: #111111;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            font-weight: 700;
            font-size: 14px;
            cursor: pointer;
            transition: background 0.2s;
        }
        button.btn:hover {
            background-color: #2d3748;
        }
        .back-link {
            display: block;
            text-align: center;
            margin-top: 16px;
            font-size: 14px;
            color: #718096;
            text-decoration: none;
            font-weight: 600;
        }
        .back-link:hover {
            color: #111111;
        }
        .status-msg {
            margin-top: 16px;
            font-weight: 700;
            font-size: 14px;
            text-align: center;
            display: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Konfigurasi Wi-Fi RoboPot</h1>
        <div class="form-group">
            <label>SSID Wi-Fi</label>
            <input type="text" id="wifi-ssid" placeholder="Masukkan SSID Wi-Fi">
        </div>
        <div class="form-group">
            <label>Password</label>
            <input type="password" id="wifi-pass" placeholder="Masukkan Password Wi-Fi">
        </div>
        <button class="btn" onclick="saveWifi()">Simpan & Hubungkan</button>
        <div id="status" class="status-msg"></div>
        <a href="/" class="back-link">← Kembali ke Dasbor</a>
    </div>

    <script>
        function saveWifi() {
            const ssid = document.getElementById('wifi-ssid').value;
            const pass = document.getElementById('wifi-pass').value;
            if (!ssid) {
                alert('SSID Wi-Fi tidak boleh kosong!');
                return;
            }
            const statusDiv = document.getElementById('status');
            statusDiv.style.display = 'block';
            statusDiv.style.color = '#111111';
            statusDiv.innerText = 'Menyimpan konfigurasi...';

            fetch(`/api/save-wifi?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`)
                .then(res => res.text())
                .then(msg => {
                    statusDiv.innerText = msg;
                    statusDiv.style.color = '#22543d';
                })
                .catch(err => {
                    statusDiv.style.color = '#742a2a';
                    statusDiv.innerText = 'Gagal menyimpan: ' + err;
                });
        }
    </script>
</body>
</html>
)rawliteral";

void scanI2CBus() {
    Wire.begin(8, 9);
    Wire.setTimeOut(100);

    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        err = Wire.endTransmission();

        if(err == 0) {
            my_add = address;
            Serial.printf("Device at: %d\n", address);
        }
    }

    if (my_add == 0) {
        Serial.println("I2C Device not found");
        my_add = 0x3C;
    } else {
        Serial.printf("Address Device: 0x%02X\n", my_add);
    }
}

void showWelcomeScreens() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 20);
    display.print("Initializing...");
    display.display();
    
    delay(2000);
    
    display.clearDisplay();
    display.setCursor(20, 20);
    display.print("Powered by: KKJ");
    display.display();

    delay(2000);
    display.clearDisplay();
}

uint8_t readSoilSensor() {
    static uint16_t raw_read_sensor = 0;
    static uint8_t read_sensor = 0;
    static uint32_t lastTime = 0;
    uint32_t nowTime = millis();

    if(nowTime - lastTime >= 1000) {
        lastTime = nowTime;
        raw_read_sensor = analogRead(sensor);
        read_sensor = constrain(map(raw_read_sensor, 0, 3825, 100, 0), 0, 100);
        Serial.printf("Raw: %d\nRead: %d%%\n", raw_read_sensor, read_sensor);
    }
    return read_sensor;
}

void updateRobotFace(uint8_t mood, bool resetMood = false) {
    static uint8_t lastMood = 99;
    
    if (resetMood) {
        lastMood = 99;
    }

    eyes.update();

    if (mood != lastMood) {
        lastMood = mood;
        switch(mood) {
            case normal:
                Serial.println("Mood: Normal");
                eyes.setMood(DEFAULT);
                break;

            case angry:
                Serial.println("Mood: Angry");
                eyes.setMood(ANGRY);
                break;

            case happy:
                Serial.println("Mood: Happy");
                eyes.setMood(HAPPY);
                break;

            case confused:
                Serial.println("Mood: Confused (Animation)");
                eyes.anim_confused();
                break;
        }
    }
}

void drawSoilMoisture(uint8_t read_sensor) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(30, 12);
    display.print("Soil Moisture");

    display.setTextSize(2);
    display.setCursor(55, 26);
    display.printf("%d%%", read_sensor);

    int barWidth = 100;
    int barHeight = 5;
    int barX = (DISPLAY_WIDTH - barWidth) / 2;
    int barY = 48;

    display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);

    int fillWidth = map(read_sensor, 0, 100, 0, barWidth - 2);
    if (fillWidth > 0) {
        display.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, SSD1306_WHITE);
    }

    display.display();
}

void performOTA() {
    Serial.printf("Starting OTA Update from URL: %s\n", otaUrl.c_str());
    WiFiClientSecure client;
    client.setInsecure();
    
    t_httpUpdate_return ret = httpUpdate.update(client, otaUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA Update failed. Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            shouldUpdate = false;
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No updates found.");
            shouldUpdate = false;
            break;
        case HTTP_UPDATE_OK:
            Serial.println("OTA Update successful. Restarting...");
            delay(1000);
            ESP.restart();
            break;
    }
}

void setup() {
    Serial.begin(115200);

    unsigned long startSerial = millis();
    while (!Serial && (millis() - startSerial < 3000)) {
        delay(10);
    }

    Serial.println("\n--- ROBOPOT ESP32-C3 Initialize ---");
    
    scanI2CBus();

    if (!display.begin(SSD1306_SWITCHCAPVCC, my_add)) {
        Serial.println("SSD1306 allocation failed");
    }
    
    showWelcomeScreens();

    eyes.begin(DISPLAY_WIDTH, DISPLAY_HEIGHT, REFRESH_RATE);
    eyes.setAutoblinker(true);
    eyes.setCuriosity(true);
    eyes.setIdleMode(true);

    eyes.close();
    eyes.open();
    Serial.println("RoboEyes Initialized!");

    pinMode(sensor, INPUT);

    preferences.begin("wifi-config", true);
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    preferences.end();

    WiFi.mode(WIFI_AP_STA);
    
    bool connected = false;
    if (wifiSSID.length() > 0) {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        unsigned long startWifi = millis();
        connected = true;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            if (millis() - startWifi > 10000) {
                connected = false;
                break;
            }
        }
    }

    if (connected) {
        Serial.println("\nConnected! IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect. Starting AP mode...");
        WiFi.softAP("RoboPot-AP", "12345678");
        Serial.println("AP Mode started. IP Address: ");
        Serial.println(WiFi.softAPIP());
    }

    if (MDNS.begin("robopot")) {
        Serial.println("mDNS responder started: http://robopot.local");
    }

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", dashboardHTML);
    });

    server.on("/config-wifi", HTTP_GET, []() {
        server.send(200, "text/html", wifiConfigHTML);
    });

    server.on("/api/save-wifi", HTTP_GET, []() {
        if (server.hasArg("ssid")) {
            String newSSID = server.arg("ssid");
            String newPass = server.arg("pass");
            preferences.begin("wifi-config", false);
            preferences.putString("ssid", newSSID);
            preferences.putString("password", newPass);
            preferences.end();
            server.send(200, "text/plain", "Konfigurasi Wi-Fi berhasil disimpan! ESP32 akan merestart dalam 2 detik...");
            shouldRestart = true;
            restartTimer = millis();
        } else {
            server.send(400, "text/plain", "Error: Parameter SSID kosong!");
        }
    });

    server.on("/api/data", HTTP_GET, []() {
        String json = "{";
        json += "\"moisture\":" + String(readSoilSensor()) + ",";
        json += "\"mood\":" + String(autoMood ? random(0, 4) : manualMood) + ",";
        json += "\"autoMood\":" + String(autoMood ? "true" : "false") + ",";
        json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
        json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
        json += "\"faceDur\":" + String(faceDuration) + ",";
        json += "\"sensorDur\":" + String(moistureDuration);
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/api/set", HTTP_GET, []() {
        if (server.hasArg("auto")) {
            autoMood = server.arg("auto") == "1";
        }
        if (server.hasArg("mood")) {
            manualMood = server.arg("mood").toInt();
        }
        if (server.hasArg("face")) {
            faceDuration = server.arg("face").toInt();
        }
        if (server.hasArg("sensor")) {
            moistureDuration = server.arg("sensor").toInt();
        }
        server.send(200, "text/plain", "Konfigurasi berhasil disimpan!");
    });

    server.on("/api/update", HTTP_GET, []() {
        if (server.hasArg("url")) {
            otaUrl = server.arg("url");
            shouldUpdate = true;
            server.send(200, "text/plain", "Mulai mengunduh file update dari GitHub...");
        } else {
            server.send(400, "text/plain", "Error: Parameter URL tidak ada!");
        }
    });

    server.begin();
    Serial.println("HTTP Server started.");
}

void loop() {
    server.handleClient();

    if (shouldRestart && (millis() - restartTimer >= 2000)) {
        ESP.restart();
    }

    if (shouldUpdate) {
        delay(2000);
        performOTA();
        return;
    }

    static uint8_t mood = 0;
    static uint32_t lastMoodTime = 0;
    uint32_t nowTime = millis();

    if (autoMood) {
        if (nowTime - lastMoodTime >= 10000) {
            lastMoodTime = nowTime;
            mood = random(0, 4);
            Serial.printf("Mengubah mood ke indeks: %d\n", mood);
        }
    } else {
        mood = manualMood;
    }

    uint8_t currentMoisture = readSoilSensor();

    static ScreenState currentState = STATE_FACE;
    static uint32_t lastStateChange = 0;
    static bool isFirstFaceLoop = false;

    if (currentState == STATE_FACE) {
        updateRobotFace(mood, isFirstFaceLoop);
        if (isFirstFaceLoop) {
            isFirstFaceLoop = false;
        }

        if (nowTime - lastStateChange >= faceDuration) {
            currentState = STATE_MOISTURE;
            lastStateChange = nowTime;
            display.clearDisplay();
        }
    } 
    else if (currentState == STATE_MOISTURE) {
        drawSoilMoisture(currentMoisture);

        if (nowTime - lastStateChange >= moistureDuration) {
            currentState = STATE_FACE;
            lastStateChange = nowTime;
            display.clearDisplay();
            isFirstFaceLoop = true;
        }
    }
}



