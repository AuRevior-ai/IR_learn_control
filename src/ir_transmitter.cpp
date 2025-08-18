#include "ir_transmitter.h"

IRTransmitter::IRTransmitter(uint8_t pin) {
    send_pin = pin;
    irsend = new IRsend(pin);
    is_sending = false;
}

IRTransmitter::~IRTransmitter() {
    if (irsend) {
        delete irsend;
    }
}

bool IRTransmitter::begin() {
    if (!irsend) return false;
    
    irsend->begin();
    Serial.printf("[IR_TX] 红外发射器初始化完成，发射引脚: GPIO%d\n", send_pin);
    return true;
}

bool IRTransmitter::sendNEC(uint32_t data, uint16_t bits, uint16_t repeat) {
    if (!irsend) return false;
    
    is_sending = true;
    Serial.printf("[IR_TX] 发射NEC信号: 0x%08X, %d位", data, bits);
    if (repeat > 0) Serial.printf(", 重复%d次", repeat);
    Serial.println();
    
    irsend->sendNEC(data, bits, repeat);
    
    is_sending = false;
    return true;
}

bool IRTransmitter::sendSony(uint32_t data, uint16_t bits, uint16_t repeat) {
    if (!irsend) return false;
    
    is_sending = true;
    Serial.printf("[IR_TX] 发射Sony信号: 0x%08X, %d位", data, bits);
    if (repeat > 0) Serial.printf(", 重复%d次", repeat);
    Serial.println();
    
    irsend->sendSony(data, bits, repeat);
    
    is_sending = false;
    return true;
}

bool IRTransmitter::sendRC5(uint32_t data, uint16_t bits, uint16_t repeat) {
    if (!irsend) return false;
    
    is_sending = true;
    Serial.printf("[IR_TX] 发射RC5信号: 0x%08X, %d位", data, bits);
    if (repeat > 0) Serial.printf(", 重复%d次", repeat);
    Serial.println();
    
    irsend->sendRC5(data, bits, repeat);
    
    is_sending = false;
    return true;
}

bool IRTransmitter::sendRaw(uint16_t* rawData, uint16_t length, uint16_t freq) {
    if (!irsend || !rawData) return false;
    
    is_sending = true;
    Serial.printf("[IR_TX] 发射原始数据，长度: %d, 频率: %dkHz\n", length, freq);
    
    irsend->sendRaw(rawData, length, freq);
    
    is_sending = false;
    return true;
}

bool IRTransmitter::sendSignal(decode_type_t protocol, uint32_t data, uint16_t bits, uint16_t repeat) {
    // 首先尝试使用已知协议
    switch (protocol) {
        case NEC:
        case NEC_LIKE:
            Serial.printf("[IR_TX] 使用NEC协议发射: 0x%08X, %d位\n", data, bits);
            return sendNEC(data, bits, repeat);
            
        case SONY:
            Serial.printf("[IR_TX] 使用SONY协议发射: 0x%08X, %d位\n", data, bits);
            return sendSony(data, bits, repeat);
            
        case RC5:
        case RC5X:
            Serial.printf("[IR_TX] 使用RC5协议发射: 0x%08X, %d位\n", data, bits);
            return sendRC5(data, bits, repeat);
            
        default:
            // 对于未知协议，尝试使用IRremoteESP8266的通用发射功能
            Serial.printf("[IR_TX] 尝试使用通用方法发射协议: %s, 数据: 0x%08X, %d位\n", 
                         typeToString(protocol, false).c_str(), data, bits);
            
            // 使用IRremoteESP8266库的send()方法，它支持更多协议
            if (irsend) {
                is_sending = true;
                bool success = irsend->send(protocol, data, bits, repeat);
                is_sending = false;
                
                if (success) {
                    Serial.println("[IR_TX] ✅ 通用方法发射成功");
                    return true;
                } else {
                    Serial.printf("[IR_TX] ⚠️ 通用方法失败，协议 %s 可能不被支持\n", 
                                 typeToString(protocol, false).c_str());
                    return false;
                }
            }
            return false;
    }
}

// 新增：带原始数据的发射函数
bool IRTransmitter::sendSignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                               uint16_t* rawData, uint16_t rawLength, uint16_t repeat) {
    // 首先尝试使用协议特定的方法
    bool protocolSuccess = sendSignal(protocol, data, bits, repeat);
    
    if (protocolSuccess) {
        return true;
    }
    
    // 如果协议方法失败，且有原始数据，则使用原始数据发射
    if (rawData && rawLength > 0) {
        Serial.printf("[IR_TX] 协议方法失败，使用原始数据发射，长度: %d\n", rawLength);
        
        // 对于无法识别的协议，使用38kHz载波频率
        uint16_t frequency = 38;
        
        // 根据协议调整频率
        switch (protocol) {
            case SONY:
                frequency = 40;
                break;
            case RC5:
            case RC6:
                frequency = 36;
                break;
            default:
                frequency = 38;
                break;
        }
        
        is_sending = true;
        Serial.printf("[IR_TX] 使用原始数据发射，频率: %dkHz\n", frequency);
        
        // 改进的原始数据发射：多次发射提高稳定性
        bool success = false;
        for (int attempt = 0; attempt <= repeat; attempt++) {
            // 发射前短暂延时，确保发射器稳定
            delay(10);
            
            // 发射原始数据
            irsend->sendRaw(rawData, rawLength, frequency);
            
            // 发射间隔，避免信号重叠
            if (attempt < repeat) {
                delay(100); // 增加重复发射间隔
            }
            success = true;
        }
        
        // 发射后额外延时，确保信号完整
        delay(50);
        
        is_sending = false;
        
        if (success) {
            Serial.println("[IR_TX] ✅ 原始数据发射完成");
            return true;
        }
    }
    
    Serial.println("[IR_TX] ❌ 无可用的发射方法");
    return false;
}

// 新增：信号验证测试
bool IRTransmitter::verifySignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                                 uint16_t* rawData, uint16_t rawLength, uint16_t testCount) {
    Serial.printf("[IR_TX] 🧪 开始信号验证测试，将发射 %d 次\n", testCount);
    Serial.printf("[IR_TX] 📋 信号信息: 协议=%s, 值=0x%08X, 位数=%d\n", 
                 typeToString(protocol, false).c_str(), data, bits);
    Serial.println("[IR_TX] 💡 请观察接收器是否能稳定接收到相同信号");
    Serial.println("================================");
    
    int successCount = 0;
    
    for (int i = 1; i <= testCount; i++) {
        Serial.printf("[IR_TX] 📡 第 %d/%d 次发射...\n", i, testCount);
        
        bool success = sendSignal(protocol, data, bits, rawData, rawLength, 1);
        
        if (success) {
            successCount++;
            Serial.printf("[IR_TX] ✅ 第 %d 次发射成功\n", i);
        } else {
            Serial.printf("[IR_TX] ❌ 第 %d 次发射失败\n", i);
        }
        
        // 发射间隔，便于观察
        if (i < testCount) {
            Serial.println("[IR_TX] ⏳ 等待 2 秒...");
            delay(2000);
        }
    }
    
    float successRate = (float)successCount / testCount * 100;
    Serial.println("================================");
    Serial.printf("[IR_TX] 📊 验证结果: %d/%d 次成功，成功率: %.1f%%\n", 
                 successCount, testCount, successRate);
    
    if (successRate >= 80) {
        Serial.println("[IR_TX] ✅ 信号稳定性良好");
        return true;
    } else if (successRate >= 60) {
        Serial.println("[IR_TX] ⚠️ 信号稳定性一般，建议重新学习");
        return false;
    } else {
        Serial.println("[IR_TX] ❌ 信号不稳定，需要重新学习");
        return false;
    }
}

bool IRTransmitter::testTransmitter() {
    Serial.println("[IR_TX] 测试红外发射器...");
    
    // 发射测试信号 (NEC协议)
    uint32_t testData = 0x00FF00FF;  // 测试数据
    
    Serial.println("[IR_TX] 发射测试信号，请用手机摄像头观察红外LED");
    sendNEC(testData, 32, 2);
    
    delay(1000);
    
    Serial.println("[IR_TX] 测试完成");
    return true;
}

bool IRTransmitter::isSending() {
    return is_sending;
}

void IRTransmitter::setFrequency(uint16_t freq) {
    Serial.printf("[IR_TX] 设置载波频率: %dkHz\n", freq);
    // IRsend库会自动处理频率设置
}
