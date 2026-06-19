const int LED_PIN = 4;

// 时间定义
const unsigned long T = 200;              // 基本时间单位
const unsigned long DOT = T;              // 点 = 1T
const unsigned long DASH = 3 * T;          // 划 = 3T
const unsigned long SYM_GAP = T;           // 符号间隔 = 1T
const unsigned long CHR_GAP = 3 * T;       // 字符间隔 = 3T
const unsigned long WORD_GAP = 7 * T;      // 词间隔 = 7T (循环停顿)

// 完整SOS时序表: [亮时长, 灭时长, 亮时长, 灭时长, ...]
// S = ···, O = ———, S = ···
const unsigned long TIMING[] = {
  // S: · · ·
  DOT, SYM_GAP, DOT, SYM_GAP, DOT, CHR_GAP,
  // O: — — —
  DASH, SYM_GAP, DASH, SYM_GAP, DASH, CHR_GAP,
  // S: · · ·
  DOT, SYM_GAP, DOT, SYM_GAP, DOT, WORD_GAP
};
const int TIMING_LENGTH = sizeof(TIMING) / sizeof(TIMING[0]);

unsigned long previousMillis = 0;
int timingIndex = 0;
bool isLedOnPhase = true;  // true=亮阶段, false=灭阶段

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 开始第一个亮
  previousMillis = millis();
  Serial.begin(115200);
  Serial.println("SOS started");
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= TIMING[timingIndex]) {
    previousMillis = currentMillis;
    
    // 切换到下一阶段
    isLedOnPhase = !isLedOnPhase;
    digitalWrite(LED_PIN, isLedOnPhase ? HIGH : LOW);
    
    timingIndex++;
    if (timingIndex >= TIMING_LENGTH) {
      timingIndex = 0;
      isLedOnPhase = true;  // 重置为亮阶段
      digitalWrite(LED_PIN, HIGH);
      Serial.println("--- New SOS ---");
    }
  }
}
