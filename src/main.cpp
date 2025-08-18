#include <Arduino.h>
#include "ir_receiver.h"
#include "ir_transmitter.h"
#include "ir_storage.h"

// 引脚定义
#define IR_RECEIVER_PIN 2    // VS1838B数据引脚
#define IR_TRANSMITTER_PIN 4 // IR333C-A控制引脚（通过三极管）
#define STATUS_LED_PIN 5     // 状态指示LED（使用GPIO5）

// 对象实例
IRReceiver irReceiver(IR_RECEIVER_PIN);
IRTransmitter irTransmitter(IR_TRANSMITTER_PIN);
IRStorage irStorage;

// 函数声明
void processCommand(String command);
void handleLearning();
void finalizeLearning(); // 新增：完成学习分析
void showHelp();
void startLearning();
void stopCurrentOperation();
void listStoredSignals();
void clearAllSignals();
void sendSignal(int id);
void repeatSignal(int id, int times);
void deleteSignal(int id);
void showSignalInfo(int id);
void showRawData(int id);
void testTransmitter();
void verifySignal(int id); // 新增：验证信号稳定性
void showDetailedSignalInfo(int id); // 新增：显示详细信号信息
void ledSignalFlash(); // 新增：信号接收LED闪烁
void ledStartupFlash(); // 新增：启动LED闪烁
void testGPIO2(); // 新增：测试GPIO2引脚状态
void diagnosePullupResistor(); // 新增：诊断上拉电阻问题

// 程序状态
enum SystemState {
  IDLE,
  LEARNING,
  TRANSMITTING
};

// 学习模式配置
struct LearningConfig {
  static const int MAX_SAMPLES = 20;      // 最大采样次数
  static const int MIN_SAMPLES = 5;       // 最少采样次数
  static const unsigned long TIMEOUT = 30000;  // 30秒超时
  static const unsigned long SAMPLE_INTERVAL = 200; // 采样间隔200ms
};

// 信号样本结构
struct SignalSample {
  uint32_t value;
  uint16_t bits;
  decode_type_t protocol;
  unsigned long timestamp;
  bool isValid;
};

SystemState currentState = IDLE;

// 学习相关变量
SignalSample learningSamples[LearningConfig::MAX_SAMPLES];
int sampleCount = 0;
unsigned long learningStartTime = 0;
unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 红外学习与控制系统");
  Serial.println("============================");
  
  // 初始化状态LED并进行启动闪烁
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // 初始化模块
  irReceiver.begin();
  irTransmitter.begin();
  irStorage.begin();
  
  Serial.println("系统初始化完成");
  
  // 启动闪烁提示
  ledStartupFlash();
  
  Serial.println("输入 'help' 查看可用命令");
  Serial.print("> ");
}

void loop() {
  // 检查串口命令 (使用简单可靠的readString方法)
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    if (command.length() > 0) {
      processCommand(command);
      Serial.print("> ");
    }
  }
  
  // 根据当前状态执行相应操作
  switch (currentState) {
    case LEARNING:
      handleLearning();
      break;
    case TRANSMITTING:
      // 发射状态在命令处理中完成
      currentState = IDLE;
      break;
    case IDLE:
    default:
      // 空闲状态，等待命令
      break;
  }
  
  delay(10);
}

void processCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  if (command == "help") {
    showHelp();
  } else if (command == "learn") {
    startLearning();
  } else if (command == "stop") {
    stopCurrentOperation();
  } else if (command == "list") {
    listStoredSignals();
  } else if (command == "clear") {
    clearAllSignals();
  } else if (command.startsWith("send ")) {
    String idStr = command.substring(5);
    idStr.trim();
    // 清理尖括号和其他非数字字符
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.replace("(", "");
    idStr.replace(")", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      sendSignal(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command.startsWith("repeat ")) {
    int spaceIndex = command.indexOf(' ', 7);
    if (spaceIndex > 0) {
      String idStr = command.substring(7, spaceIndex);
      String timesStr = command.substring(spaceIndex + 1);
      idStr.trim();
      timesStr.trim();
      // 清理尖括号
      idStr.replace("<", "");
      idStr.replace(">", "");
      idStr.replace("[", "");
      idStr.replace("]", "");
      timesStr.replace("<", "");
      timesStr.replace(">", "");
      timesStr.replace("[", "");
      timesStr.replace("]", "");
      idStr.trim();
      timesStr.trim();
      int id = idStr.toInt();
      int times = timesStr.toInt();
      if (id > 0 && times > 0) {
        repeatSignal(id, times);
      } else {
        Serial.println("错误: 无效的参数，请输入正整数");
      }
    } else {
      Serial.println("错误: repeat命令格式为 'repeat <id> <times>'");
    }
  } else if (command.startsWith("delete ")) {
    String idStr = command.substring(7);
    idStr.trim();
    // 清理尖括号
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      deleteSignal(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command.startsWith("info ")) {
    String idStr = command.substring(5);
    idStr.trim();
    // 清理尖括号
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      showSignalInfo(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command.startsWith("detail ")) {
    String idStr = command.substring(7);
    idStr.trim();
    // 清理尖括号
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      showDetailedSignalInfo(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command.startsWith("raw ")) {
    String idStr = command.substring(4);
    idStr.trim();
    // 清理尖括号
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      showRawData(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command == "test") {
    testTransmitter();
  } else if (command.startsWith("verify ")) {
    String idStr = command.substring(7);
    idStr.trim();
    // 清理尖括号
    idStr.replace("<", "");
    idStr.replace(">", "");
    idStr.replace("[", "");
    idStr.replace("]", "");
    idStr.trim();
    int id = idStr.toInt();
    if (id > 0) {
      verifySignal(id);
    } else {
      Serial.println("错误: 无效的信号ID，请输入正整数");
    }
  } else if (command == "gpio") {
    testGPIO2();
  } else if (command == "diag") {
    diagnosePullupResistor();
  } else {
    Serial.println("未知命令，输入 'help' 查看可用命令");
  }
}

void showHelp() {
  bool isLearning = (currentState == LEARNING);
  
  Serial.println("\n📚 可用命令：");
  Serial.println("🔧 基础命令：");
  Serial.println("  help         - 显示此帮助信息");
  Serial.println("  learn        - 进入学习模式");
  Serial.println("  stop         - 停止当前操作");
  Serial.println("  list         - 列出已学习的信号");
  Serial.println("  clear        - 清除所有已学习信号");
  Serial.println("\n📡 发射命令：");
  if (isLearning) {
    Serial.println("  send <id>    - 🎯 在学习模式下测试发射信号(不退出学习)");
    Serial.println("  test         - 🎯 在学习模式下测试发射器(不退出学习)");
  } else {
    Serial.println("  send <id>    - 发射指定ID的信号(带重试机制)");
    Serial.println("  test         - 测试发射器功能");
  }
  Serial.println("  repeat <id> <times> - 重复发射信号");
  Serial.println("  delete <id>  - 删除指定ID的信号");
  Serial.println("\n🔍 调试命令：");
  Serial.println("  info <id>    - 显示信号基本信息");
  Serial.println("  detail <id>  - 🆕 显示超详细信号解析(含NEC协议完整分析)");
  Serial.println("  raw <id>     - 显示原始信号数据");
  Serial.println("  verify <id>  - 🆕 验证信号稳定性(连续发射测试)");
  Serial.println("  gpio         - 🆕 测试GPIO2引脚精确电压");
  Serial.println("  diag         - 🆕 诊断上拉电阻问题");
  
  if (isLearning) {
    Serial.println("\n🎯 学习模式提示：");
    Serial.println("  💡 可在学习过程中使用 send 和 test 命令测试发射");
    Serial.println("  📡 发射后观察是否能接收到相同信号验证功能");
    Serial.println("  🔄 学习模式下发射不会退出学习状态");
  }
  
  Serial.println();
}

void startLearning() {
  Serial.println("🎯 进入智能学习模式...");
  Serial.println("================================");
  Serial.printf("📊 配置: 需要采集 %d-%d 个样本\n", LearningConfig::MIN_SAMPLES, LearningConfig::MAX_SAMPLES);
  Serial.printf("⏱️ 超时时间: %d 秒\n", LearningConfig::TIMEOUT / 1000);
  Serial.println("📡 请将遥控器对准接收器(距离5-10cm)");
  Serial.println("🔄 请连续按下同一个按键 5-20 次");
  Serial.println("💡 系统会自动分析并选择最稳定的信号");
  Serial.println("🛑 输入 'stop' 可随时退出学习模式");
  Serial.println("================================");
  
  // 重置学习状态
  sampleCount = 0;
  learningStartTime = millis();
  lastSampleTime = 0;
  memset(learningSamples, 0, sizeof(learningSamples));
  
  currentState = LEARNING;
  digitalWrite(STATUS_LED_PIN, HIGH); // 点亮LED表示学习模式
}

void handleLearning() {
  unsigned long currentTime = millis();
  
  // 检查超时
  if (currentTime - learningStartTime > LearningConfig::TIMEOUT) {
    Serial.println("⏰ 学习超时，正在分析已收集的数据...");
    if (sampleCount >= LearningConfig::MIN_SAMPLES) {
      finalizeLearning();
    } else {
      Serial.printf("❌ 样本不足（%d < %d），学习失败\n", sampleCount, LearningConfig::MIN_SAMPLES);
      stopCurrentOperation();
    }
    return;
  }
  
  if (irReceiver.isAvailable() && irReceiver.decode()) {
    // 防抖：避免重复采样
    if (currentTime - lastSampleTime < LearningConfig::SAMPLE_INTERVAL) {
      return;
    }
    
    // 获取接收到的信号数据
    uint32_t value = irReceiver.getValue();
    uint16_t bits = irReceiver.getBits();
    decode_type_t protocol = irReceiver.getProtocol();
    
    // 验证信号有效性
    if (value == 0 || bits == 0) {
      Serial.println("⚠️ 无效信号，请重试");
      return;
    }
    
    // 添加样本
    if (sampleCount < LearningConfig::MAX_SAMPLES) {
      learningSamples[sampleCount].value = value;
      learningSamples[sampleCount].bits = bits;
      learningSamples[sampleCount].protocol = protocol;
      learningSamples[sampleCount].timestamp = currentTime;
      learningSamples[sampleCount].isValid = true;
      
      sampleCount++;
      lastSampleTime = currentTime;
      
      Serial.printf("✅ 样本 %d/%d: 协议=%s, 值=0x%08X, 位数=%d\n", 
                   sampleCount, LearningConfig::MAX_SAMPLES,
                   typeToString(protocol, false).c_str(), value, bits);
      
      // 信号接收成功时LED快闪一次
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(50);
      digitalWrite(STATUS_LED_PIN, HIGH);
      
      // 达到最小样本数后提示可以结束
      if (sampleCount == LearningConfig::MIN_SAMPLES) {
        Serial.println("💡 已达到最小样本数，可输入 'stop' 结束学习");
      }
      
      // 达到最大样本数时自动结束
      if (sampleCount >= LearningConfig::MAX_SAMPLES) {
        Serial.println("📊 已达到最大样本数，正在分析数据...");
        finalizeLearning();
      }
    }
  }
}

// 新增：完成学习并分析数据
void finalizeLearning() {
  if (sampleCount < LearningConfig::MIN_SAMPLES) {
    Serial.printf("❌ 样本不足（%d < %d），学习失败\n", sampleCount, LearningConfig::MIN_SAMPLES);
    currentState = IDLE;
    digitalWrite(STATUS_LED_PIN, LOW);
    sampleCount = 0;
    return;
  }
  
  Serial.println("\n🔍 开始信号分析...");
  Serial.println("================================");
  
  // 使用简化的分析方法，减少栈使用
  uint32_t bestValue = 0;
  uint16_t bestBits = 0;
  decode_type_t bestProtocol = UNKNOWN;
  int maxCount = 0;
  int bestIndex = 0;
  
  // 为每个样本计算出现次数
  for (int i = 0; i < sampleCount; i++) {
    if (!learningSamples[i].isValid) continue;
    
    int count = 1;
    // 计算当前样本在所有样本中的出现次数
    for (int j = i + 1; j < sampleCount; j++) {
      if (learningSamples[j].isValid &&
          learningSamples[j].value == learningSamples[i].value &&
          learningSamples[j].bits == learningSamples[i].bits &&
          learningSamples[j].protocol == learningSamples[i].protocol) {
        count++;
        learningSamples[j].isValid = false; // 标记为已处理，避免重复计算
      }
    }
    
    // 如果这个信号出现次数更多，更新最佳选择
    if (count > maxCount) {
      maxCount = count;
      bestValue = learningSamples[i].value;
      bestBits = learningSamples[i].bits;
      bestProtocol = learningSamples[i].protocol;
      bestIndex = i;
    }
    
    float percentage = (float)count / sampleCount * 100;
    Serial.printf("📊 信号: 0x%08X (%s, %d位) - 出现 %d 次 (%.1f%%)\n", 
                 learningSamples[i].value, 
                 typeToString(learningSamples[i].protocol, false).c_str(),
                 learningSamples[i].bits, count, percentage);
  }
  
  float reliability = (float)maxCount / sampleCount * 100;
  Serial.printf("\n🎯 选择最稳定信号: 0x%08X (可靠性: %.1f%%)\n", bestValue, reliability);
  
  // 获取原始数据 - 使用最后接收的数据
  uint16_t* rawData = irReceiver.getRawData();
  uint16_t rawLength = irReceiver.getRawLength();
  
  // 生成信号名称
  char signalName[48];
  sprintf(signalName, "Signal_%d_R%.0f%%", irStorage.getSignalCount() + 1, reliability);
  
  // 存储最佳信号
  int id = irStorage.addSignal(bestProtocol, bestValue, bestBits, rawData, rawLength, signalName);
  
  if (id > 0) {
    Serial.printf("✅ 学习成功！信号已保存为ID: %d\n", id);
    Serial.printf("📋 信号详情: %s, 值: 0x%08X, 位数: %d\n", 
                 typeToString(bestProtocol, false).c_str(), bestValue, bestBits);
    
    // 成功时LED闪烁3次
    for (int i = 0; i < 3; i++) {
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(100);
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(100);
    }
  } else {
    Serial.println("❌ 存储失败！存储空间可能已满");
  }
  
  Serial.println("================================");
  
  // 直接清理状态，避免递归调用
  currentState = IDLE;
  digitalWrite(STATUS_LED_PIN, LOW);
  sampleCount = 0;
  learningStartTime = 0;
  lastSampleTime = 0;
  Serial.println("✅ 学习完成");
}

void stopCurrentOperation() {
  // 防止在学习分析过程中重复调用
  if (currentState == LEARNING && sampleCount >= LearningConfig::MIN_SAMPLES) {
    Serial.printf("🔄 学习中断，但已收集 %d 个样本，正在分析...\n", sampleCount);
    finalizeLearning();
    return;
  }
  
  if (currentState == LEARNING && sampleCount < LearningConfig::MIN_SAMPLES) {
    Serial.printf("❌ 学习中断，样本不足（%d < %d）\n", sampleCount, LearningConfig::MIN_SAMPLES);
  }
  
  // 清理状态
  currentState = IDLE;
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // 清理学习状态
  sampleCount = 0;
  learningStartTime = 0;
  lastSampleTime = 0;
  
  Serial.println("✅ 操作已停止");
}

void listStoredSignals() {
  Serial.println("\n已学习的信号列表：");
  Serial.println("ID | 协议         | 值         | 位数 | 名称");
  Serial.println("---|-------------|------------|------|----------");
  
  int count = irStorage.getSignalCount();
  if (count == 0) {
    Serial.println("暂无已学习的信号");
  } else {
    for (int i = 1; i <= count; i++) {
      IRSignal* signal = irStorage.getSignal(i);
      if (signal && signal->isValid) {
        Serial.printf("%2d | %-11s | 0x%08X | %4d | %s\n", 
                     i, typeToString(signal->protocol, false).c_str(), signal->value, 
                     signal->bits, signal->name);
      }
    }
  }
  Serial.println();
}

void clearAllSignals() {
  irStorage.clearAll();
  Serial.println("所有信号已清除");
}

void sendSignal(int id) {
  IRSignal* signal = irStorage.getSignal(id);
  if (signal && signal->isValid) {
    Serial.printf("📡 发射信号 ID: %d (%s)\n", id, signal->name);
    Serial.printf("📋 协议: %s, 值: 0x%08X, 位数: %d\n", 
                 typeToString(signal->protocol, false).c_str(), signal->value, signal->bits);
    
    // 保存当前状态，避免在学习模式下改变状态
    SystemState previousState = currentState;
    bool wasLearning = (currentState == LEARNING);
    
    if (!wasLearning) {
      currentState = TRANSMITTING;
    }
    
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    // 改进的重试机制
    bool success = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
      Serial.printf("🔄 尝试发射第 %d 次...\n", attempt);
      
      // 使用优化的发射参数：增加重复次数提高稳定性
      success = irTransmitter.sendSignal(signal->protocol, signal->value, signal->bits,
                                        signal->rawData, signal->rawLength, 2);
      
      if (success) {
        Serial.printf("✅ 第 %d 次发射成功！\n", attempt);
        break;
      } else {
        Serial.printf("❌ 第 %d 次发射失败\n", attempt);
        if (attempt < 3) {
          delay(300); // 增加重试间隔
        }
      }
    }
    
    // 恢复之前的状态
    if (wasLearning) {
      currentState = LEARNING;
      Serial.println("🎯 继续学习模式，请继续按遥控器测试接收...");
    } else {
      digitalWrite(STATUS_LED_PIN, LOW);
      currentState = IDLE;
    }
    
    if (success) {
      Serial.println("✅ 发射完成");
    } else {
      Serial.println("❌ 发射失败 - 重试3次后仍然失败");
    }
  } else {
    Serial.printf("❌ 错误: 信号 ID %d 不存在\n", id);
  }
}

void repeatSignal(int id, int times) {
  IRSignal* signal = irStorage.getSignal(id);
  if (signal && signal->isValid) {
    Serial.printf("重复发射信号 ID: %d，次数: %d\n", id, times);
    currentState = TRANSMITTING;
    
    for (int i = 0; i < times; i++) {
      digitalWrite(STATUS_LED_PIN, HIGH);
      bool success = irTransmitter.sendSignal(signal->protocol, signal->value, signal->bits,
                                             signal->rawData, signal->rawLength, 0);
      digitalWrite(STATUS_LED_PIN, LOW);
      
      if (!success) {
        Serial.printf("第 %d 次发射失败\n", i + 1);
        break;
      }
      
      Serial.printf("第 %d 次发射完成\n", i + 1);
      delay(300); // 增加间隔
    }
    Serial.println("重复发射完成");
  } else {
    Serial.printf("错误: 信号 ID %d 不存在\n", id);
  }
}

void deleteSignal(int id) {
  if (irStorage.deleteSignal(id)) {
    Serial.printf("信号 ID %d 已删除\n", id);
  } else {
    Serial.printf("错误: 信号 ID %d 不存在\n", id);
  }
}

void showSignalInfo(int id) {
  IRSignal* signal = irStorage.getSignal(id);
  if (signal && signal->isValid) {
    Serial.printf("\n信号 ID %d 详细信息：\n", id);
    Serial.printf("协议: %s\n", typeToString(signal->protocol, false).c_str());
    Serial.printf("值: 0x%08X (%u)\n", signal->value, signal->value);
    Serial.printf("位数: %d\n", signal->bits);
    Serial.printf("原始数据长度: %d\n", signal->rawLength);
    Serial.printf("名称: %s\n", signal->name);
    Serial.printf("学习时间: %lu\n", signal->timestamp);
  } else {
    Serial.printf("错误: 信号 ID %d 不存在\n", id);
  }
  Serial.println();
}

void showRawData(int id) {
  IRSignal* signal = irStorage.getSignal(id);
  if (signal && signal->isValid) {
    Serial.printf("\n信号 ID %d 原始数据：\n", id);
    Serial.printf("信号值: 0x%08X\n", signal->value);
    Serial.printf("二进制: ");
    for (int i = signal->bits - 1; i >= 0; i--) {
      Serial.print((signal->value >> i) & 1);
      if (i % 8 == 0 && i > 0) Serial.print(" ");
    }
    Serial.println();
    
    if (signal->rawLength > 0) {
      Serial.println("原始时序数据:");
      for (int i = 0; i < signal->rawLength && i < 256; i++) {
        Serial.printf("%4d ", signal->rawData[i]);
        if ((i + 1) % 10 == 0) Serial.println();
      }
      Serial.println();
    }
  } else {
    Serial.printf("错误: 信号 ID %d 不存在\n", id);
  }
  Serial.println();
}

void testTransmitter() {
  bool wasLearning = (currentState == LEARNING);
  
  Serial.println("🧪 测试发射器功能...");
  Serial.println("📡 发射测试信号 (NEC协议, 值:0xFF00FF)");
  
  if (wasLearning) {
    Serial.println("🎯 学习模式下测试，请观察是否能接收到发射的信号...");
  }
  
  digitalWrite(STATUS_LED_PIN, HIGH);
  bool success = irTransmitter.sendSignal(NEC, 0xFF00FF, 32);
  
  if (!wasLearning) {
    digitalWrite(STATUS_LED_PIN, LOW);
  }
  
  if (success) {
    Serial.println("✅ 测试完成 - 请用手机摄像头观察IR LED是否闪烁");
    if (wasLearning) {
      Serial.println("🔍 如果学习模式中能接收到此信号，说明收发功能正常");
    }
  } else {
    Serial.println("❌ 测试失败 - 请检查硬件连接");
  }
}

// 显示超详细的信号信息（包含NEC协议的完整解析）
void showDetailedSignalInfo(int id) {
  IRSignal* signal = irStorage.getSignal(id);
  if (!signal || !signal->isValid) {
    Serial.printf("❌ 错误: 信号 ID %d 不存在\n", id);
    return;
  }

  Serial.println("\n" + String("=").substring(0, 60));
  Serial.printf("🔍 信号 ID %d 超详细分析\n", id);
  Serial.println(String("=").substring(0, 60));
  
  // 基础信息
  Serial.printf("📋 基础信息:\n");
  Serial.printf("   协议: %s\n", typeToString(signal->protocol, false).c_str());
  Serial.printf("   信号名称: %s\n", signal->name);
  Serial.printf("   学习时间: %lu\n", signal->timestamp);
  Serial.printf("   数据长度: %d 位\n", signal->bits);
  Serial.printf("   原始数据长度: %d\n", signal->rawLength);
  
  // 十六进制值显示
  Serial.printf("\n💾 数据值:\n");
  Serial.printf("   HEX: 0x%08X\n", signal->value);
  Serial.printf("   DEC: %u\n", signal->value);
  
  // 二进制显示（按字节分组）
  Serial.printf("\n🔢 二进制数据 (%d位):\n", signal->bits);
  Serial.print("   BIN: ");
  for (int i = signal->bits - 1; i >= 0; i--) {
    Serial.print((signal->value >> i) & 1);
    if (i % 8 == 0 && i > 0) Serial.print(" ");
    if (i % 32 == 0 && i > 0) Serial.print("\n        ");
  }
  Serial.println();
  
  // NEC协议专门解析
  if (signal->protocol == NEC && signal->bits == 32) {
    Serial.printf("\n🎯 NEC协议详细解析:\n");
    uint8_t address = (signal->value >> 24) & 0xFF;
    uint8_t addressInv = (signal->value >> 16) & 0xFF;
    uint8_t command = (signal->value >> 8) & 0xFF;
    uint8_t commandInv = signal->value & 0xFF;
    
    Serial.printf("   地址码: 0x%02X (%d)\n", address, address);
    Serial.printf("   地址反码: 0x%02X (%d) %s\n", addressInv, addressInv, 
                 (address == (~addressInv & 0xFF)) ? "✅" : "❌");
    Serial.printf("   命令码: 0x%02X (%d)\n", command, command);
    Serial.printf("   命令反码: 0x%02X (%d) %s\n", commandInv, commandInv,
                 (command == (~commandInv & 0xFF)) ? "✅" : "❌");
    
    // 检查数据完整性
    bool addressValid = (address == (~addressInv & 0xFF));
    bool commandValid = (command == (~commandInv & 0xFF));
    Serial.printf("   数据完整性: %s\n", 
                 (addressValid && commandValid) ? "✅ 完整" : "❌ 损坏");
  }
  
  // 原始时序数据显示
  if (signal->rawLength > 0) {
    Serial.printf("\n📊 原始时序数据 (%d个数据点):\n", signal->rawLength);
    Serial.println("   格式: [索引] 持续时间(μs) 类型");
    
    for (int i = 0; i < signal->rawLength && i < 100; i++) { // 限制显示前100个数据点
      if (i % 5 == 0) Serial.printf("\n   ");
      Serial.printf("[%02d]%4d%s ", i, signal->rawData[i], (i % 2 == 0) ? "H" : "L");
    }
    
    if (signal->rawLength > 100) {
      Serial.printf("\n   ... (还有 %d 个数据点未显示)", signal->rawLength - 100);
    }
    Serial.println();
    
    // 分析时序特征
    Serial.printf("\n📈 时序特征分析:\n");
    uint16_t minVal = 65535, maxVal = 0;
    uint32_t totalTime = 0;
    for (int i = 0; i < signal->rawLength; i++) {
      if (signal->rawData[i] < minVal) minVal = signal->rawData[i];
      if (signal->rawData[i] > maxVal) maxVal = signal->rawData[i];
      totalTime += signal->rawData[i];
    }
    Serial.printf("   最短脉冲: %d μs\n", minVal);
    Serial.printf("   最长脉冲: %d μs\n", maxVal);
    Serial.printf("   总持续时间: %u μs (%.1f ms)\n", totalTime, totalTime / 1000.0);
    Serial.printf("   平均脉冲长度: %d μs\n", totalTime / signal->rawLength);
  }
  
  Serial.println(String("=").substring(0, 60));
  Serial.println();
}

// LED控制函数
void ledStartupFlash() {
  // 启动时快速闪烁3次，然后熄灭
  Serial.println("🔆 系统启动中...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(150);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(150);
  }
  Serial.println("✅ 系统就绪");
}

void ledSignalFlash() {
  // 信号接收时闪烁两下
  for (int i = 0; i < 2; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(100);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(100);
  }
}

// 新增：GPIO2精确电压测试函数
void testGPIO2() {
  Serial.println("📟 GPIO2(VS1838B OUT)精确电压测试:");
  Serial.println("===========================================");
  
  // 配置ADC
  analogReadResolution(12);  // 设置12位精度 (0-4095)
  analogSetAttenuation(ADC_11db);  // 设置衰减，测量范围0-3.3V
  
  Serial.println("测量方式: 数字读取 | 模拟读取(ADC) | 计算电压");
  Serial.println("-------------------------------------------");
  
  for (int i = 0; i < 20; i++) {
    // 数字读取
    int digitalReading = digitalRead(2);
    
    // 模拟读取 (ESP32的GPIO2也可以作为ADC2_CH2使用)
    int analogReading = analogRead(2);
    
    // 计算实际电压 (12位ADC，参考电压3.3V)
    float voltage = (analogReading * 3.3) / 4095.0;
    
    Serial.printf("读数 %2d: 数字=%d | ADC=%4d | 电压=%.3fV", 
                  i+1, digitalReading, analogReading, voltage);
    
    // 状态判断
    if (voltage > 2.5) {
      Serial.println(" ✅ 高电平");
    } else if (voltage < 0.8) {
      Serial.println(" ⬇️ 低电平");
    } else {
      Serial.println(" ⚠️ 中间电平(异常)");
    }
    
    delay(500);
  }
  
  Serial.println("===========================================");
  Serial.println("📊 电压分析:");
  Serial.println("  > 2.5V: 正常高电平 ✅");
  Serial.println("  < 0.8V: 正常低电平 ⬇️");
  Serial.println("  0.8V-2.5V: 异常中间电平 ⚠️ (需要上拉电阻)");
  Serial.println("测试完成！\n");
}

// 新增：验证信号稳定性
void verifySignal(int id) {
  IRSignal* signal = irStorage.getSignal(id);
  if (!signal || !signal->isValid) {
    Serial.printf("错误: 信号 ID %d 不存在\n", id);
    return;
  }
  
  Serial.printf("🧪 开始验证信号 ID: %d (%s)\n", id, signal->name);
  Serial.printf("📋 协议: %s, 值: 0x%08X, 位数: %d\n", 
               typeToString(signal->protocol, false).c_str(), signal->value, signal->bits);
  
  // 保存当前状态，验证后恢复
  SystemState previousState = currentState;
  bool wasLearning = (currentState == LEARNING);
  
  currentState = TRANSMITTING;
  
  // 使用发射器的验证功能
  bool result = irTransmitter.verifySignal(signal->protocol, signal->value, signal->bits,
                                          signal->rawData, signal->rawLength, 5);
  
  // 恢复状态
  currentState = previousState;
  
  if (wasLearning) {
    Serial.println("🎯 继续学习模式，请继续按遥控器测试接收...");
  }
  
  if (result) {
    Serial.printf("✅ 信号 ID %d 验证通过，稳定性良好\n", id);
  } else {
    Serial.printf("⚠️ 信号 ID %d 验证失败，建议重新学习\n", id);
  }
}

// 新增：诊断上拉电阻问题
void diagnosePullupResistor() {
  Serial.println("🔧 VS1838B上拉电阻诊断");
  Serial.println("================================");
  
  // 配置ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  Serial.println("步骤1: 测试GPIO2浮空状态");
  pinMode(2, INPUT);  // 设置为输入，无上拉
  delay(100);
  int floating = analogRead(2);
  float floatingV = (floating * 3.3) / 4095.0;
  Serial.printf("浮空电压: %.3fV (ADC=%d)\n", floatingV, floating);
  
  Serial.println("\n步骤2: 测试内部上拉状态");
  pinMode(2, INPUT_PULLUP);  // 启用内部上拉
  delay(100);
  int pullup = analogRead(2);
  float pullupV = (pullup * 3.3) / 4095.0;
  Serial.printf("内部上拉电压: %.3fV (ADC=%d)\n", pullupV, pullup);
  
  Serial.println("\n步骤3: 连续监测5秒(请准备遥控器测试)");
  for (int i = 0; i < 10; i++) {
    int reading = analogRead(2);
    float voltage = (reading * 3.3) / 4095.0;
    Serial.printf("时刻 %ds: %.3fV ", i+1, voltage);
    
    if (voltage > 2.8) {
      Serial.println("✅ 理想高电平");
    } else if (voltage > 2.0) {
      Serial.println("⚠️ 偏低高电平");
    } else if (voltage < 0.5) {
      Serial.println("📶 信号检测");
    } else {
      Serial.println("❌ 异常电平");
    }
    delay(500);
  }
  
  Serial.println("\n📋 诊断结果:");
  if (pullupV > 2.8) {
    Serial.println("✅ 内部上拉工作正常");
    Serial.println("💡 建议: 添加外部4.7kΩ上拉电阻以获得更好性能");
  } else if (pullupV > 2.0) {
    Serial.println("⚠️ 内部上拉偏弱");
    Serial.println("🔧 建议: 必须添加外部1-4.7kΩ上拉电阻");
  } else {
    Serial.println("❌ 内部上拉异常或VS1838B有问题");
    Serial.println("🔧 建议: 检查接线和VS1838B工作状态");
  }
  
  Serial.println("\n🔗 硬件连接建议:");
  Serial.println("3.3V ----[4.7kΩ]---- GPIO2 ---- VS1838B OUT");
  Serial.println("================================\n");
}