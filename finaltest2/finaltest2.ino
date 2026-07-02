#include <WiFi.h>
#include <WebServer.h>

// ========== AP 热点配置 ==========
const char* ap_ssid = "ESP32-LAB210";
const char* ap_pass = "12345678";

// ========== 三个LED引脚配置 ==========
#define LED1_PIN 2
#define LED2_PIN 4
#define LED3_PIN 5

const int pwmFreq = 5000;
const int pwmResolution = 8;

// ========== Web服务器 ==========
WebServer server(80);

// ========== 返回主页面（三个滑动条 - 优化版） ==========
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 三通道无极调光器</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin-top: 40px;
      background: #f5f5f5;
    }
    h1 { color: #333; }
    .container {
      background: white;
      padding: 30px;
      border-radius: 12px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      display: inline-block;
      min-width: 340px;
      max-width: 90%;
    }
    .channel {
      margin: 20px 0;
      padding: 15px;
      border-radius: 8px;
      background: #fafafa;
    }
    .ch-title {
      font-size: 16px;
      font-weight: bold;
      margin-bottom: 10px;
    }
    .ch1 { color: #e53935; }
    .ch2 { color: #43a047; }
    .ch3 { color: #1e88e5; }
    input[type=range] {
      width: 85%;
      height: 36px;
      margin: 8px 0;
      cursor: pointer;
    }
    .value-display {
      font-size: 32px;
      font-weight: bold;
      margin: 5px 0;
    }
    .status {
      color: #666;
      font-size: 12px;
      margin-top: 5px;
      height: 16px;
    }
    .live-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #4CAF50;
      margin-right: 6px;
      animation: blink 1s infinite;
    }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.3} }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔆 三通道无极调光器</h1>
    <p><span class="live-indicator"></span>实时模式 - 拖动即响应</p>
    
    <div class="channel">
      <div class="ch-title ch1">🔴 LED 1 (GPIO2)</div>
      <div class="value-display ch1" id="valBox0">0</div>
      <input type="range" id="slider0" min="0" max="255" value="0">
      <div class="status" id="status0">就绪</div>
    </div>
    
    <div class="channel">
      <div class="ch-title ch2">🟢 LED 2 (GPIO4)</div>
      <div class="value-display ch2" id="valBox1">0</div>
      <input type="range" id="slider1" min="0" max="255" value="0">
      <div class="status" id="status1">就绪</div>
    </div>
    
    <div class="channel">
      <div class="ch-title ch3">🔵 LED 3 (GPIO5)</div>
      <div class="value-display ch3" id="valBox2">0</div>
      <input type="range" id="slider2" min="0" max="255" value="0">
      <div class="status" id="status2">就绪</div>
    </div>
  </div>

  <script>
    // 使用 AbortController 取消旧请求，保证只发送最新值
    const controllers = [null, null, null];
    const pending = [false, false, false];
    
    function sendFast(ch, val) {
      // 取消上一个未完成的请求
      if (controllers[ch]) {
        controllers[ch].abort();
      }
      controllers[ch] = new AbortController();
      
      // 使用 no-cors + keepalive 快速发送，不等待响应
      fetch('/set?ch=' + ch + '&val=' + val, {
        method: 'GET',
        signal: controllers[ch].signal,
        keepalive: true
      }).catch(() => {}); // 忽略所有错误，不阻塞UI
    }
    
    // 为每个滑动条绑定事件
    for (let i = 0; i < 3; i++) {
      const slider = document.getElementById('slider' + i);
      const valBox = document.getElementById('valBox' + i);
      const status = document.getElementById('status' + i);
      
      // input 事件：拖动过程中实时触发
      slider.addEventListener('input', function(e) {
        const val = e.target.value;
        valBox.innerText = val;
        status.innerText = '亮度: ' + val;
        status.style.color = '#2196F3';
        
        sendFast(i, val);
      });
      
      // change 事件：拖动结束后确认
      slider.addEventListener('change', function(e) {
        status.innerText = '已设置: ' + e.target.value;
        status.style.color = '#4CAF50';
      });
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=UTF-8", html);
}

// ========== 处理 /set 请求：极简快速响应 ==========
void handleSet() {
  if (server.hasArg("ch") && server.hasArg("val")) {
    int ch = server.arg("ch").toInt();
    int val = server.arg("val").toInt();
    val = constrain(val, 0, 255);
    
    int pin;
    switch (ch) {
      case 0: pin = LED1_PIN; break;
      case 1: pin = LED2_PIN; break;
      case 2: pin = LED3_PIN; break;
      default: server.send(200, "text/plain", "OK"); return;
    }
    
    ledcWrite(pin, val);
    
    // 立即返回，不做任何额外处理
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "OK");
  }
}

// ========== 404 快速返回 ==========
void handleNotFound() {
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  
  // 初始化三个PWM通道
  ledcAttach(LED1_PIN, pwmFreq, pwmResolution);
  ledcAttach(LED2_PIN, pwmFreq, pwmResolution);
  ledcAttach(LED3_PIN, pwmFreq, pwmResolution);
  
  ledcWrite(LED1_PIN, 0);
  ledcWrite(LED2_PIN, 0);
  ledcWrite(LED3_PIN, 0);
  
  // 开启AP热点
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  
  Serial.println("\n=== 三通道实时调光器已启动 ===");
  Serial.print("热点名称: ");
  Serial.println(ap_ssid);
  Serial.print("访问地址: http://");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web服务器已启动");
}

void loop() {
  server.handleClient();
}