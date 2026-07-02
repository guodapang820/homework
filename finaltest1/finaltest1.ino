#include <WiFi.h>
#include <WebServer.h>

// ========== AP 热点配置 ==========
const char* ap_ssid = "ESP32-LAB210";
const char* ap_pass = "12345678";  // 至少8位

// ========== LED 与 PWM 配置 ==========
#define LED_PIN 2               // LED连接D2 (GPIO2)
const int pwmFreq = 5000;       // PWM频率 5kHz
const int pwmResolution = 8;    // 8位分辨率 (0-255)

// ========== Web服务器 ==========
WebServer server(80);           // 监听80端口

// 当前亮度值
int currentBrightness = 0;

// ========== 处理根路径：返回带文本输入框的网页 ==========
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 无极调光器</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin-top: 60px;
      background: #f5f5f5;
    }
    h1 { color: #333; }
    .container {
      background: white;
      padding: 30px;
      border-radius: 12px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      display: inline-block;
      min-width: 300px;
    }
    input[type=number] {
      width: 120px;
      height: 44px;
      font-size: 24px;
      text-align: center;
      border: 2px solid #ddd;
      border-radius: 8px;
      outline: none;
      transition: border-color 0.3s;
    }
    input[type=number]:focus {
      border-color: #2196F3;
    }
    button {
      width: 120px;
      height: 44px;
      font-size: 16px;
      margin-left: 10px;
      background: #2196F3;
      color: white;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: background 0.3s;
    }
    button:hover {
      background: #1976D2;
    }
    button:active {
      background: #0D47A1;
    }
    .value-display {
      font-size: 48px;
      font-weight: bold;
      color: #2196F3;
      margin: 20px 0;
    }
    .hint {
      color: #999;
      font-size: 13px;
      margin-top: 8px;
    }
    .status {
      color: #666;
      font-size: 14px;
      margin-top: 15px;
      min-height: 20px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 无极调光器</h1>
    <p>在文本框中输入亮度值 (0~255)</p>
    
    <div class="value-display" id="valBox">0</div>
    
    <div>
      <input type="number" id="pwmInput" min="0" max="255" value="0" 
             placeholder="0-255" onkeydown="if(event.key==='Enter')sendPWM()">
      <button onclick="sendPWM()">设置亮度</button>
    </div>
    
    <div class="hint">按 Enter 键或点击按钮确认</div>
    
    <div class="status" id="statusText">等待操作...</div>
  </div>

  <script>
    function sendPWM() {
      var input = document.getElementById('pwmInput');
      var val = parseInt(input.value);
      
      // 校验范围
      if (isNaN(val) || val < 0 || val > 255) {
        document.getElementById('statusText').innerText = '请输入 0~255 之间的整数';
        document.getElementById('statusText').style.color = '#f44336';
        return;
      }
      
      document.getElementById('valBox').innerText = val;
      document.getElementById('statusText').innerText = '发送中...';
      document.getElementById('statusText').style.color = '#666';
      
      fetch('/set?val=' + val)
        .then(response => {
          if (response.ok) {
            document.getElementById('statusText').innerText = '亮度已更新: ' + val;
            document.getElementById('statusText').style.color = '#4CAF50';
          } else {
            document.getElementById('statusText').innerText = '发送失败';
            document.getElementById('statusText').style.color = '#f44336';
          }
        })
        .catch(err => {
          document.getElementById('statusText').innerText = '网络错误';
          document.getElementById('statusText').style.color = '#f44336';
          console.error(err);
        });
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=UTF-8", html);
}

// ========== 处理 /set 请求：解析亮度值并设置PWM ==========
void handleSet() {
  if (server.hasArg("val")) {
    String valStr = server.arg("val");
    int brightness = valStr.toInt();
    
    brightness = constrain(brightness, 0, 255);
    currentBrightness = brightness;
    
    ledcWrite(LED_PIN, brightness);
    
    Serial.print("收到亮度值: ");
    Serial.println(brightness);
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameter");
  }
}

// ========== 处理 404 ==========
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  Serial.begin(115200);
  
  // 初始化 PWM
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0);  // 初始熄灭
  
  // ========== 开启 AP 热点模式 ==========
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  
  Serial.println("AP热点已开启");
  Serial.print("热点名称: ");
  Serial.println(ap_ssid);
  Serial.print("访问地址: http://");
  Serial.println(WiFi.softAPIP());  // 通常 192.168.4.1
  
  // 注册路由
  server.on("/", handleRoot);      // 主页（文本输入页面）
  server.on("/set", handleSet);    // 调光接口
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web服务器已启动");
}

void loop() {
  server.handleClient();
}