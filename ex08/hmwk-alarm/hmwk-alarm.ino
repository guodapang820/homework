#include <WiFi.h>
#include <WebServer.h>

// ========== AP 热点配置 ==========
const char* ap_ssid = "ESP32-LAB210";
const char* ap_pass = "12345678";

// ========== 引脚与PWM配置 ==========
#define TOUCH_PIN 4
#define LED_PIN 2
const int pwmFreq = 5000;
const int pwmResolution = 8;

// ========== 系统状态定义 ==========
const int STATE_DISARMED = 0;   // 撤防
const int STATE_ARMED = 1;      // 布防
const int STATE_ALARMING = 2;   // 报警中
volatile int systemState = STATE_DISARMED;

// ========== 触摸检测变量（边缘检测 + 软件防抖） ==========
bool lastTouchState = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;  // 防抖延时 200ms
const int touchThreshold = 200;            // 触摸阈值（根据串口打印值调整）

// ========== 报警闪烁变量（非阻塞式） ==========
unsigned long lastAlarmToggle = 0;
const int alarmInterval = 100;  // 100ms切换一次，每秒10次狂闪
bool alarmLedState = false;

// ========== Web服务器 ==========
WebServer server(80);

// 获取状态文本
String getStateText(int state) {
  switch (state) {
    case STATE_DISARMED: return "撤防中 (Disarmed)";
    case STATE_ARMED: return "布防中 (Armed)";
    case STATE_ALARMING: return "报警中 (ALARM!)";
    default: return "未知";
  }
}

// ========== 返回主页面（布防/撤防按钮 + 状态显示） ==========
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 物联网安防报警器</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 40px 20px;
      background: #f5f5f5;
      transition: background 0.3s;
    }
    .container {
      max-width: 400px;
      margin: 0 auto;
      background: white;
      padding: 30px;
      border-radius: 16px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }
    h1 { color: #333; margin-bottom: 10px; }
    .subtitle { color: #666; font-size: 14px; margin-bottom: 30px; }
    #statusBox {
      font-size: 20px;
      font-weight: bold;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 30px;
      background: #e0e0e0;
      color: #555;
      transition: all 0.3s;
    }
    .status-armed { background: #c8e6c9 !important; color: #2e7d32 !important; }
    .status-alarm { background: #ffcdd2 !important; color: #c62828 !important; animation: pulse 0.5s infinite alternate; }
    @keyframes pulse { from { opacity: 1; } to { opacity: 0.7; } }
    .btn {
      display: block;
      width: 100%;
      padding: 18px;
      margin: 12px 0;
      font-size: 18px;
      font-weight: bold;
      border: none;
      border-radius: 10px;
      cursor: pointer;
      transition: transform 0.1s, opacity 0.2s;
    }
    .btn:active { transform: scale(0.98); }
    .btn-arm { background: #4CAF50; color: white; }
    .btn-arm:hover { background: #45a049; }
    .btn-disarm { background: #f44336; color: white; }
    .btn-disarm:hover { background: #da190b; }
    #alarmBox {
      display: none;
      margin-top: 20px;
      font-size: 48px;
      animation: shake 0.3s infinite;
    }
    @keyframes shake { 0% { transform: translate(0,0); } 25% { transform: translate(-5px,0); } 50% { transform: translate(5px,0); } 75% { transform: translate(-5px,0); } 100% { transform: translate(0,0); } }
    .info { margin-top: 20px; font-size: 12px; color: #999; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🏠 安防主机</h1>
    <div class="subtitle">ESP32 物联网安防报警器</div>
    
    <div id="statusBox">加载中...</div>
    
    <button class="btn btn-arm" onclick="arm()">🔒 布防 (Arm)</button>
    <button class="btn btn-disarm" onclick="disarm()">🔓 撤防 (Disarm)</button>
    
    <div id="alarmBox">🚨 入侵报警！</div>
    
    <div class="info">触摸引脚: GPIO4 | LED: GPIO2</div>
  </div>

  <script>
    function updateUI(data) {
      const box = document.getElementById('statusBox');
      const alarm = document.getElementById('alarmBox');
      const body = document.body;
      
      box.innerText = '当前状态: ' + data.text;
      box.className = '';
      
      if (data.state == 0) {
        body.style.background = '#f5f5f5';
        alarm.style.display = 'none';
      } else if (data.state == 1) {
        box.classList.add('status-armed');
        body.style.background = '#e8f5e9';
        alarm.style.display = 'none';
      } else if (data.state == 2) {
        box.classList.add('status-alarm');
        body.style.background = '#ffebee';
        alarm.style.display = 'block';
      }
    }
    
    function arm() {
      fetch('/arm').then(r => r.text()).then(() => updateStatus());
    }
    
    function disarm() {
      fetch('/disarm').then(r => r.text()).then(() => updateStatus());
    }
    
    function updateStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(data => updateUI(data))
        .catch(err => console.error(err));
    }
    
    // 页面加载时更新状态，并每秒轮询
    updateStatus();
    setInterval(updateStatus, 1000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=UTF-8", html);
}

// ========== 布防 ==========
void handleArm() {
  if (systemState == STATE_DISARMED) {
    systemState = STATE_ARMED;
    lastTouchState = false;  // 重置触摸状态，防止布防瞬间误触发
    Serial.println("用户操作: 布防");
    server.send(200, "text/plain", "Armed");
  } else {
    server.send(200, "text/plain", "Already Armed or Alarming");
  }
}

// ========== 撤防 ==========
void handleDisarm() {
  systemState = STATE_DISARMED;
  alarmLedState = false;
  ledcWrite(LED_PIN, 0);  // LED强制熄灭
  Serial.println("用户操作: 撤防，系统重置");
  server.send(200, "text/plain", "Disarmed");
}

// ========== 查询状态（JSON） ==========
void handleStatus() {
  String json = "{\"state\":";
  json += systemState;
  json += ",\"text\":\"";
  json += getStateText(systemState);
  json += "\"}";
  server.send(200, "application/json", json);
}

// ========== 404 ==========
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  Serial.begin(115200);
  
  // 初始化PWM
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0);  // 初始熄灭
  
  // 开启AP热点
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
  
  Serial.println("\n=== 物联网安防报警器已启动 ===");
  Serial.print("热点名称: ");
  Serial.println(ap_ssid);
  Serial.print("访问地址: http://");
  Serial.println(WiFi.softAPIP());
  
  // 注册路由
  server.on("/", handleRoot);
  server.on("/arm", handleArm);
  server.on("/disarm", handleDisarm);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web服务器已启动");
}

void loop() {
  // 处理Web客户端请求
  server.handleClient();
  
  unsigned long currentTime = millis();
  
  // ========== 触摸检测（仅在布防状态下执行） ==========
  if (systemState == STATE_ARMED) {
    int touchValue = touchRead(TOUCH_PIN);
    bool currentTouchState = (touchValue < touchThreshold);
    
    // 边缘检测：上一次未触摸，当前被触摸（按下瞬间）
    if (currentTouchState && !lastTouchState) {
      // 软件防抖：距离上次有效触发超过 debounceDelay
      if (currentTime - lastDebounceTime > debounceDelay) {
        lastDebounceTime = currentTime;
        systemState = STATE_ALARMING;
        Serial.println("⚠️ 触摸触发！进入报警状态！");
      }
    }
    lastTouchState = currentTouchState;
  }
  
  // ========== 报警闪烁处理（非阻塞式） ==========
  if (systemState == STATE_ALARMING) {
    if (currentTime - lastAlarmToggle >= alarmInterval) {
      lastAlarmToggle = currentTime;
      alarmLedState = !alarmLedState;
      ledcWrite(LED_PIN, alarmLedState ? 255 : 0);
    }
  } else if (systemState == STATE_DISARMED) {
    // 撤防状态：确保LED熄灭
    if (alarmLedState) {
      alarmLedState = false;
      ledcWrite(LED_PIN, 0);
    }
  } else if (systemState == STATE_ARMED) {
    // 布防状态：LED微弱常亮作为布防指示灯（如不需要可改为0）
    ledcWrite(LED_PIN, 30);
  }
  
  // 调试打印：每2秒输出一次触摸值和系统状态
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 2000) {
    lastDebugTime = currentTime;
    Serial.print("触摸值: ");
    Serial.print(touchRead(TOUCH_PIN));
    Serial.print(" | 系统状态: ");
    Serial.println(getStateText(systemState));
  }
}