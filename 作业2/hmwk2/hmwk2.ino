const int LED_PIN = 4;           // LED连接管脚
const unsigned long INTERVAL = 500; // 状态切换间隔(毫秒)，500ms = 0.5秒

unsigned long previousMillis = 0; // 记录上次状态切换时间
bool ledState = LOW;              // 当前LED状态

void setup() {
  pinMode(LED_PIN, OUTPUT);       // 设置管脚为输出模式
  digitalWrite(LED_PIN, ledState); // 初始状态熄灭
  Serial.begin(115200);           // 可选：开启串口调试
  Serial.println("LED Blink with millis() started");
}

void loop() {
  unsigned long currentMillis = millis(); // 获取当前运行毫秒数
  
  // 检查是否到达切换时间
  if (currentMillis - previousMillis >= INTERVAL) {
    previousMillis = currentMillis; // 更新上次切换时间
    
    ledState = !ledState;           // 翻转LED状态
    digitalWrite(LED_PIN, ledState); // 输出新状态
    
    //串口打印状态
    Serial.print("LED State: ");
    Serial.println(ledState ? "ON" : "OFF");
  }
}
