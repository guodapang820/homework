#include <WiFi.h>
#include <WebServer.h>

// ========== AP 热点配置 ==========
const char* ap_ssid = "ESP32-LAB210";
const char* ap_pass = "12345678";

// ========== 触摸引脚配置 ==========
#define TOUCH_PIN 4

// ========== Web服务器 ==========
WebServer server(80);

// ========== 返回主页面（仪表盘） ==========
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 实时传感器仪表盘</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #0a0e27;
      color: #fff;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
    }
    .header {
      position: absolute;
      top: 30px;
      text-align: center;
    }
    .header h1 { font-size: 28px; font-weight: 300; letter-spacing: 2px; }
    .header p { color: #6b7c9c; margin-top: 8px; font-size: 14px; }
    .dashboard {
      position: relative;
      width: 320px;
      height: 320px;
    }
    .gauge-bg {
      width: 280px;
      height: 280px;
      border-radius: 50%;
      border: 8px solid #1a2342;
      position: absolute;
      top: 20px;
      left: 20px;
      box-shadow: 0 0 40px rgba(0,150,255,0.1);
    }
    .gauge-fill {
      width: 280px;
      height: 280px;
      border-radius: 50%;
      border: 8px solid transparent;
      border-top-color: #00d4ff;
      border-right-color: #00d4ff;
      position: absolute;
      top: 20px;
      left: 20px;
      transform: rotate(-135deg);
      transition: transform 0.15s ease;
    }
    .value-display {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      text-align: center;
    }
    .value-number {
      font-size: 72px;
      font-weight: 700;
      color: #00d4ff;
      font-family: 'Courier New', monospace;
      text-shadow: 0 0 20px rgba(0,212,255,0.5);
      transition: color 0.15s;
    }
    .value-label {
      font-size: 14px;
      color: #6b7c9c;
      margin-top: 8px;
      letter-spacing: 3px;
    }
    .status-bar {
      position: absolute;
      bottom: 60px;
      display: flex;
      gap: 30px;
      align-items: center;
    }
    .status-item {
      text-align: center;
    }
    .status-value {
      font-size: 18px;
      color: #00ff88;
      font-weight: 600;
    }
    .status-text {
      font-size: 12px;
      color: #6b7c9c;
      margin-top: 4px;
    }
    .update-time {
      position: absolute;
      bottom: 30px;
      font-size: 12px;
      color: #4a5568;
    }
    .touch-indicator {
      position: absolute;
      bottom: 100px;
      padding: 8px 20px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: 600;
      transition: all 0.15s;
      opacity: 0;
    }
    .touch-indicator.active {
      opacity: 1;
      background: rgba(255,50,50,0.2);
      color: #ff4444;
      border: 1px solid #ff4444;
    }
    .touch-indicator.inactive {
      opacity: 1;
      background: rgba(0,255,136,0.1);
      color: #00ff88;
      border: 1px solid #00ff88;
    }
    @keyframes pulse-ring {
      0% { transform: scale(1); opacity: 0.8; }
      100% { transform: scale(1.3); opacity: 0; }
    }
    .pulse {
      position: absolute;
      width: 280px;
      height: 280px;
      border-radius: 50%;
      border: 2px solid #00d4ff;
      top: 20px;
      left: 20px;
      animation: pulse-ring 1s infinite;
      pointer-events: none;
    }
    .scale-mark {
      position: absolute;
      font-size: 11px;
      color: #4a5568;
      font-family: 'Courier New', monospace;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🔬 实时传感器仪表盘</h1>
    <p>ESP32 触摸传感器数据监控面板 (5Hz 高频采样)</p>
  </div>
  
  <div class="dashboard">
    <div class="gauge-bg"></div>
    <div class="gauge-fill" id="gaugeFill"></div>
    <div class="pulse" id="pulseRing"></div>
    <div class="value-display">
      <div class="value-number" id="sensorValue">--</div>
      <div class="value-label">TOUCH VALUE</div>
    </div>
    <div class="scale-mark" style="bottom:25px;left:50%;transform:translateX(-50%);">500</div>
    <div class="scale-mark" style="top:20px;left:50%;transform:translateX(-50%);">1000</div>
    <div class="scale-mark" style="bottom:25px;left:20px;">0</div>
    <div class="scale-mark" style="bottom:25px;right:20px;">1000</div>
  </div>
  
  <div class="touch-indicator" id="touchStatus">等待数据...</div>
  
  <div class="status-bar">
    <div class="status-item">
      <div class="status-value" id="rssiValue">-</div>
      <div class="status-text">信号强度</div>
    </div>
    <div class="status-item">
      <div class="status-value" id="updateRate">5Hz</div>
      <div class="status-text">刷新频率</div>
    </div>
    <div class="status-item">
      <div class="status-value" id="sampleCount">0</div>
      <div class="status-text">采样次数</div>
    </div>
  </div>
  
  <div class="update-time" id="lastUpdate">等待连接...</div>

  <script>
    let sampleCount = 0;
    
    function updateGauge(value) {
      const maxVal = 1000;  // 量程上限 1000
      const minVal = 0;
      const clamped = Math.max(minVal, Math.min(maxVal, value));
      const percentage = (clamped - minVal) / (maxVal - minVal);
      const angle = -135 + (percentage * 270);  // -135° 到 135°
      document.getElementById('gaugeFill').style.transform = `rotate(${angle}deg)`;
    }
    
    // 阈值修改：<300 红色（触摸中）、<700 黄色、>700 蓝色
    function updateColor(value) {
      const num = document.getElementById('sensorValue');
      const pulse = document.getElementById('pulseRing');
      if (value < 300) {
        // 触摸中 - 红色
        num.style.color = '#ff4444';
        num.style.textShadow = '0 0 20px rgba(255,68,68,0.5)';
        pulse.style.borderColor = '#ff4444';
      } else if (value < 700) {
        // 过渡区 - 黄色
        num.style.color = '#ffaa00';
        num.style.textShadow = '0 0 20px rgba(255,170,0,0.5)';
        pulse.style.borderColor = '#ffaa00';
      } else {
        // 未触摸 - 蓝色
        num.style.color = '#00d4ff';
        num.style.textShadow = '0 0 20px rgba(0,212,255,0.5)';
        pulse.style.borderColor = '#00d4ff';
      }
    }
    
    // 触摸状态阈值同步改为 <300
    function updateTouchStatus(value) {
      const status = document.getElementById('touchStatus');
      if (value < 300) {
        status.className = 'touch-indicator active';
        status.innerText = '🔴 触摸检测中';
      } else {
        status.className = 'touch-indicator inactive';
        status.innerText = '🟢 未触摸';
      }
    }
    
    async function fetchData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        
        const value = data.touch;
        document.getElementById('sensorValue').innerText = value;
        
        updateGauge(value);
        updateColor(value);
        updateTouchStatus(value);
        
        sampleCount++;
        document.getElementById('sampleCount').innerText = sampleCount;
        document.getElementById('rssiValue').innerText = data.rssi + 'dBm';
        document.getElementById('lastUpdate').innerText = '上次更新: ' + new Date().toLocaleTimeString();
        
      } catch (err) {
        document.getElementById('lastUpdate').innerText = '连接失败: ' + err.message;
      }
    }
    
    // 高频采样：每 200ms 拉取一次，即 5Hz
    fetchData();
    setInterval(fetchData, 200);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=UTF-8", html);
}

// ========== 返回JSON传感器数据 ==========
void handleData() {
  int touchValue = touchRead(TOUCH_PIN);
  int rssi = WiFi.RSSI();
  
  String json = "{\"touch\":";
  json += touchValue;
  json += ",\"rssi\":";
  json += rssi;
  json += ",\"timestamp\":";
  json += millis();
  json += "}";
  
  server.send(200, "application/json", json);
}

// ========== 404 ==========
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  
  Serial.println("\n=== 实时传感器Web仪表盘已启动 ===");
  Serial.print("热点名称: ");
  Serial.println(ap_ssid);
  Serial.print("访问地址: http://");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web服务器已启动");
}

void loop() {
  server.handleClient();
}