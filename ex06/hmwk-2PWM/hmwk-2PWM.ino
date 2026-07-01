#define LED_A_PIN 2   // 警灯A（蓝色/红色）
#define LED_B_PIN 4   // 警灯B（红色/蓝色）

// PWM 参数
const int freq = 5000;       // 5kHz 频率
const int resolution = 8;    // 8位分辨率 (0-255)

// 呼吸状态变量
int dutyCycle = 0;           // 当前基准占空比 0~255
bool directionUp = true;     // true=渐亮, false=渐暗
unsigned long lastStepTime = 0;  // 上次步进时间戳

// 步进间隔：数值越小，交替闪烁越快（警灯效果通常较快）
const int stepDelay = 4;     // 每4ms改变一次占空比，约2秒完成一个交替周期

void setup() {
  Serial.begin(115200);
  
  // 初始化两个独立PWM通道（新版API直接绑定引脚）
  ledcAttach(LED_A_PIN, freq, resolution);
  ledcAttach(LED_B_PIN, freq, resolution);
  
  // 初始状态：A灭 B亮
  ledcWrite(LED_A_PIN, 0);
  ledcWrite(LED_B_PIN, 255);
  
  Serial.println("双通道反相警灯已启动");
  Serial.println("A灯与B灯呈平滑反相交替闪烁");
}

void loop() {
  unsigned long currentTime = millis();
  
  // 非阻塞式PWM步进
  if (currentTime - lastStepTime >= stepDelay) {
    lastStepTime = currentTime;
    
    // 更新基准占空比
    if (directionUp) {
      dutyCycle++;
      if (dutyCycle >= 255) {
        dutyCycle = 255;
        directionUp = false;  // 到达峰值，切换方向
      }
    } else {
      dutyCycle--;
      if (dutyCycle <= 0) {
        dutyCycle = 0;
        directionUp = true;   // 到达谷底，切换方向
      }
    }
    
    // ========== 核心反相逻辑 ==========
    // A灯跟随基准占空比
    ledcWrite(LED_A_PIN, dutyCycle);
    
    // B灯与A灯严格反相：A亮则B暗，A暗则B亮
    ledcWrite(LED_B_PIN, 255 - dutyCycle);
  }
  
  // 可选：每2秒打印一次当前占空比，方便调试观察
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 2000) {
    lastDebugTime = currentTime;
    Serial.print("A占空比: ");
    Serial.print(dutyCycle);
    Serial.print(" | B占空比: ");
    Serial.println(255 - dutyCycle);
  }
}