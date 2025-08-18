#include "ir_receiver.h"

IRReceiver::IRReceiver(uint8_t pin) {
    receive_pin = pin;
    irrecv = new IRrecv(pin);
    is_learning = false;
    last_receive_time = 0;
}

IRReceiver::~IRReceiver() {
    if (irrecv) {
        delete irrecv;
    }
}

bool IRReceiver::begin() {
    if (!irrecv) return false;
    
    // 重要：设置GPIO为输入模式并启用内部上拉电阻
    pinMode(receive_pin, INPUT_PULLUP);
    
    irrecv->enableIRIn();
    Serial.printf("[IR_RX] 红外接收器初始化完成，监听引脚: GPIO%d (已启用内部上拉)\n", receive_pin);
    return true;
}

bool IRReceiver::isAvailable() {
    if (!irrecv) return false;
    return irrecv->decode(&results);
}

bool IRReceiver::decode() {
    if (!isAvailable()) return false;
    
    // 检查是否是重复信号
    unsigned long now = millis();
    if (now - last_receive_time < 200) {
        irrecv->resume();
        return false;
    }
    
    last_receive_time = now;
    
    // 使用IRremoteESP8266的高级功能进行协议分析
    if (is_learning) {
        Serial.println("[IR_RX] 接收到红外信号:");
        printAdvancedResult();
    }
    
    irrecv->resume();
    return true;
}

// 新增：高级结果显示，利用IRremoteESP8266的完整功能
void IRReceiver::printAdvancedResult() {
    Serial.printf("  协议: %s", typeToString(results.decode_type, false).c_str());
    
    // 显示协议特定信息
    switch (results.decode_type) {
        case NEC:
        case NEC_LIKE:
            if (results.bits == 32) {
                uint8_t address = (results.value >> 24) & 0xFF;
                uint8_t command = (results.value >> 8) & 0xFF;
                Serial.printf(" (地址: 0x%02X, 命令: 0x%02X)", address, command);
            }
            break;
        case SONY:
            if (results.bits >= 12) {
                uint8_t command = results.value & 0x7F;
                uint8_t address = (results.value >> 7) & 0x1F;
                Serial.printf(" (地址: 0x%02X, 命令: 0x%02X)", address, command);
            }
            break;
        case RC5:
        case RC6:
            if (results.bits >= 13) {
                uint8_t address = (results.value >> 6) & 0x1F;
                uint8_t command = results.value & 0x3F;
                Serial.printf(" (地址: 0x%02X, 命令: 0x%02X)", address, command);
            }
            break;
        default:
            break;
    }
    Serial.println();
    
    Serial.printf("  数值: 0x%08X\n", (uint32_t)results.value);
    Serial.printf("  位数: %d\n", results.bits);
    Serial.printf("  原始长度: %d\n", results.rawlen);
    
    // 显示信号质量评估
    if (results.overflow) {
        Serial.println("  ⚠️ 警告: 缓冲区溢出，信号可能不完整");
    }
    
    if (results.repeat) {
        Serial.println("  🔄 检测到重复信号");
    }
    
    // 显示协议置信度
    String protocolStr = typeToString(results.decode_type, false);
    if (protocolStr != "UNKNOWN") {
        Serial.println("  ✅ 协议识别成功");
    } else {
        Serial.println("  ❓ 未知协议，使用原始数据");
    }
    
    // 限制原始数据显示长度
    if (results.rawlen > 0 && results.rawlen <= 200) {
        Serial.print("  原始数据: ");
        uint16_t maxDisplay = (results.rawlen < 50) ? results.rawlen : 50;
        for (uint16_t i = 1; i < maxDisplay; i++) {
            Serial.printf("%d", results.rawbuf[i] * kRawTick);
            if (i < maxDisplay - 1) Serial.print(",");
        }
        if (results.rawlen > 50) {
            Serial.printf("... (共%d个数据点)", results.rawlen);
        }
        Serial.println();
    }
}

void IRReceiver::startLearning() {
    is_learning = true;
    Serial.println("[IR_RX] 进入学习模式，请按下遥控器按键...");
}

void IRReceiver::stopLearning() {
    is_learning = false;
    Serial.println("[IR_RX] 退出学习模式");
}

bool IRReceiver::isLearning() {
    return is_learning;
}

uint32_t IRReceiver::getValue() {
    return results.value;
}

uint16_t IRReceiver::getBits() {
    return results.bits;
}

decode_type_t IRReceiver::getProtocol() {
    return results.decode_type;
}

String IRReceiver::getProtocolName() {
    return typeToString(results.decode_type, false);
}

uint16_t* IRReceiver::getRawData() {
    return const_cast<uint16_t*>(results.rawbuf);
}

uint16_t IRReceiver::getRawLength() {
    return results.rawlen;
}

void IRReceiver::printResult() {
    Serial.printf("  协议: %s\n", typeToString(results.decode_type, false).c_str());
    Serial.printf("  数值: 0x%08X\n", (uint32_t)results.value);
    Serial.printf("  位数: %d\n", results.bits);
    Serial.printf("  原始长度: %d\n", results.rawlen);
    
    // 打印原始数据
    Serial.print("  原始数据: ");
    for (uint16_t i = 1; i < results.rawlen; i++) {
        Serial.printf("%d", results.rawbuf[i] * 50);  // 转换为微秒
        if (i < results.rawlen - 1) Serial.print(",");
    }
    Serial.println();
}

String IRReceiver::getResultString() {
    String result = "";
    result += "Protocol: " + typeToString(results.decode_type, false);
    result += ", Value: 0x" + String((uint32_t)results.value, HEX);
    result += ", Bits: " + String(results.bits);
    return result;
}

void IRReceiver::reset() {
    if (irrecv) {
        irrecv->resume();
    }
}
