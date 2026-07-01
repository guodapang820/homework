#define TOUCH_PIN 4
#define LED_PIN 2
#define THRESHOLD 200  // 触摸阈值

// 状态变量
volatile bool touchInterruptFlag = false;  // 中断触发标志（ISR与loop通信）
bool ledState = false;                     // LED当前状态（true=亮，false=灭）
bool lastTouchState = false;               // 上一次触摸状态（用于边缘检测）

// 防抖变量
unsigned long lastDebounceTime = 0;        // 上次有效触发的时间戳
const unsigned long debounceDelay = 200;   // 防抖延时200毫秒（防止手抖误触发）

// 中断服务函数 (ISR) - 只做极简操作：设置标志位
// 不在中断里翻转LED，避免触摸期间多次触发导致状态乱跳
void IRAM_ATTR gotTouch() {
  touchInterruptFlag = true;  // 通知主循环：触摸中断被触发
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // 初始状态：LED熄灭
  
  // 绑定中断函数（当触摸值低于THRESHOLD时触发）
  touchAttachInterrupt(TOUCH_PIN, gotTouch, THRESHOLD);
  
  Serial.println("触摸自锁开关已启动");
  Serial.println("轻触D4引脚切换LED状态");
}

void loop() {
  // 读取当前触摸值（用于调试和状态判断）
  int touchValue = touchRead(TOUCH_PIN);
  
  // 判断当前是否被触摸：触摸值低于阈值表示有触摸
  bool currentTouchState = (touchValue < THRESHOLD);
  
  // ========== 边缘检测 + 软件防抖 ==========
  // 条件1：边缘检测 —— 上一次未触摸，当前被触摸（按下瞬间）
  // 条件2：软件防抖 —— 距离上次有效触发超过debounceDelay
  if (currentTouchState && !lastTouchState) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;  // 更新上次有效触发时间
      
      // 只有在"按下瞬间"才翻转LED状态（自锁逻辑）
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      
      Serial.print("触摸触发！触摸值=");
      Serial.print(touchValue);
      Serial.print(" | LED状态: ");
      Serial.println(ledState ? "亮" : "灭");
    }
  }
  
  // 更新上一次触摸状态，为下一次边缘检测做准备
  lastTouchState = currentTouchState;
  
  // 清除中断标志（如果本次循环中被触发了的话）
  if (touchInterruptFlag) {
    touchInterruptFlag = false;
  }
  
  // 可选：每500ms打印一次触摸值，方便调试时观察阈值是否合适
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 500) {
    lastPrintTime = millis();
    Serial.print("当前触摸值: ");
    Serial.println(touchValue);
  }
  
  delay(10);  // 10ms轮询间隔，平衡响应速度和CPU占用
}