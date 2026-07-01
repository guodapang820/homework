#define TOUCH_PIN 4
#define LED_PIN 2
#define THRESHOLD 200  // 触摸阈值，根据实际串口打印值调整

// ========== PWM 呼吸灯设置 ==========
const int freq = 5000;        // PWM频率 5kHz
const int resolution = 8;     // 8位分辨率 (0-255)

// ========== 速度档位系统 ==========
// 档位 1(慢速) -> 2(中速) -> 3(快速) -> 回到1
volatile int speedLevel = 1;  // 当前档位，volatile因为可能在ISR中涉及
const int MAX_LEVEL = 3;

// 各档位对应的呼吸步进间隔(ms)：数值越小，呼吸越快
// 索引0占位，1/2/3对应三个档位
const int breathDelays[4] = {0, 40, 15, 3};

// ========== 触摸自锁开关变量 ==========
volatile bool touchInterruptFlag = false;  // 中断标志
bool lastTouchState = false;               // 上一次触摸状态（边缘检测用）
unsigned long lastDebounceTime = 0;        // 上次有效触发时间
const unsigned long debounceDelay = 200;   // 软件防抖 200ms

// ========== 非阻塞呼吸灯状态变量 ==========
int dutyCycle = 0;              // 当前占空比 0-255
bool breathDirectionUp = true;  // true=渐亮(占空比增加), false=渐暗(占空比减小)
unsigned long lastBreathTime = 0;  // 上次呼吸步进的时间戳

// 中断服务函数 (ISR) - 极简操作，仅设置标志
void IRAM_ATTR gotTouch() {
  touchInterruptFlag = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 初始化LED和PWM
  pinMode(LED_PIN, OUTPUT);
  ledcAttach(LED_PIN, freq, resolution);
  ledcWrite(LED_PIN, 0);  // 初始熄灭
  
  // 绑定触摸中断
  touchAttachInterrupt(TOUCH_PIN, gotTouch, THRESHOLD);
  
  Serial.println("多档位触摸调速呼吸灯已启动");
  Serial.println("触摸D4切换速度: 1档(慢) -> 2档(中) -> 3档(快) -> 循环");
}

void loop() {
  unsigned long currentTime = millis();
  
  // ========== 1. 触摸检测：边缘检测 + 软件防抖 ==========
  int touchValue = touchRead(TOUCH_PIN);
  bool currentTouchState = (touchValue < THRESHOLD);
  
  // 边缘检测：只有"上一次未触摸，当前被触摸"的瞬间才判定为有效按下
  if (currentTouchState && !lastTouchState) {
    // 软件防抖：距离上次有效触发超过 debounceDelay 才响应
    if (currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      
      // 档位循环切换：1 -> 2 -> 3 -> 1
      speedLevel++;
      if (speedLevel > MAX_LEVEL) speedLevel = 1;
      
      // 串口反馈当前档位
      Serial.print("【触摸触发】档位切换至 ");
      Serial.print(speedLevel);
      Serial.print(" - ");
      switch(speedLevel) {
        case 1: Serial.print("慢速呼吸 (40ms/步)"); break;
        case 2: Serial.print("中速呼吸 (15ms/步)"); break;
        case 3: Serial.print("快速呼吸 (3ms/步)"); break;
      }
      Serial.println();
    }
  }
  
  // 更新上一次触摸状态，为下一次边缘检测做准备
  lastTouchState = currentTouchState;
  
  // 清除中断标志
  if (touchInterruptFlag) {
    touchInterruptFlag = false;
  }
  
  // ========== 2. 非阻塞PWM呼吸灯 ==========
  // 根据当前档位获取步进间隔
  int stepDelay = breathDelays[speedLevel];
  
  // 非阻塞式判断：只有当达到步进间隔时才改变占空比
  if (currentTime - lastBreathTime >= stepDelay) {
    lastBreathTime = currentTime;
    
    if (breathDirectionUp) {
      // 渐亮阶段：占空比增加
      dutyCycle++;
      if (dutyCycle >= 255) {
        dutyCycle = 255;
        breathDirectionUp = false;  // 到达最亮，切换为渐暗
      }
    } else {
      // 渐暗阶段：占空比减小
      dutyCycle--;
      if (dutyCycle <= 0) {
        dutyCycle = 0;
        breathDirectionUp = true;   // 到达最暗，切换为渐亮
      }
    }
    
    // 输出PWM波形
    ledcWrite(LED_PIN, dutyCycle);
  }
  
  // ========== 3. 调试信息（每2秒打印一次） ==========
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 2000) {
    lastDebugTime = currentTime;
    Serial.print("触摸值: ");
    Serial.print(touchValue);
    Serial.print(" | 档位: ");
    Serial.print(speedLevel);
    Serial.print(" | 当前占空比: ");
    Serial.println(dutyCycle);
  }
}