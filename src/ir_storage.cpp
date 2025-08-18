#include "ir_storage.h"
#include <IRutils.h>

IRStorage::IRStorage() {
    signal_count = 0;
    // 初始化信号数组
    for (int i = 0; i < MAX_SIGNALS; i++) {
        signals[i].isValid = false;
    }
}

bool IRStorage::begin() {
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("[Storage] EEPROM初始化失败!");
        return false;
    }
    
    loadFromEEPROM();
    Serial.printf("[Storage] 存储器初始化完成，已加载%d个信号\n", signal_count);
    return true;
}

void IRStorage::loadFromEEPROM() {
    // 检查魔数
    if (EEPROM.read(0) != MAGIC_NUMBER) {
        Serial.println("[Storage] EEPROM数据无效，初始化为空");
        signal_count = 0;
        return;
    }
    
    // 读取信号数量
    signal_count = EEPROM.read(1);
    if (signal_count > MAX_SIGNALS) {
        signal_count = 0;
        return;
    }
    
    // 读取信号数据
    int addr = 2;
    for (int i = 0; i < signal_count; i++) {
        EEPROM.get(addr, signals[i]);
        addr += sizeof(IRSignal);
    }
    
    Serial.printf("[Storage] 从EEPROM加载了%d个信号\n", signal_count);
}

void IRStorage::saveToEEPROM() {
    // 写入魔数
    EEPROM.write(0, MAGIC_NUMBER);
    
    // 写入信号数量
    EEPROM.write(1, signal_count);
    
    // 写入信号数据
    int addr = 2;
    for (int i = 0; i < signal_count; i++) {
        if (signals[i].isValid) {
            EEPROM.put(addr, signals[i]);
            addr += sizeof(IRSignal);
        }
    }
    
    EEPROM.commit();
    Serial.printf("[Storage] 已保存%d个信号到EEPROM\n", signal_count);
}

int IRStorage::findEmptySlot() {
    for (int i = 0; i < MAX_SIGNALS; i++) {
        if (!signals[i].isValid) {
            return i;
        }
    }
    return -1;  // 没有空闲槽位
}

int IRStorage::addSignal(decode_type_t protocol, uint32_t value, uint16_t bits, 
                        uint16_t* rawData, uint16_t rawLength, const char* name) {
    int slot = findEmptySlot();
    if (slot == -1) {
        Serial.println("[Storage] 存储空间已满!");
        return -1;
    }
    
    // 填充信号数据
    signals[slot].isValid = true;
    signals[slot].protocol = protocol;
    signals[slot].value = value;
    signals[slot].bits = bits;
    signals[slot].rawLength = min(rawLength, (uint16_t)256);
    signals[slot].timestamp = millis();
    
    // 复制原始数据
    if (rawData && rawLength > 0) {
        memcpy(signals[slot].rawData, rawData, signals[slot].rawLength * sizeof(uint16_t));
    }
    
    // 设置名称
    if (name) {
        strncpy(signals[slot].name, name, 31);
        signals[slot].name[31] = '\0';
    } else {
        snprintf(signals[slot].name, 32, "Signal_%d", slot + 1);
    }
    
    signal_count++;
    saveToEEPROM();
    
    Serial.printf("[Storage] 信号已保存到槽位%d: %s\n", slot + 1, signals[slot].name);
    return slot + 1;  // 返回1开始的ID
}

bool IRStorage::deleteSignal(int id) {
    int index = id - 1;  // 转换为0开始的索引
    if (!isValidId(id)) {
        return false;
    }
    
    signals[index].isValid = false;
    signal_count--;
    saveToEEPROM();
    
    Serial.printf("[Storage] 已删除信号ID: %d\n", id);
    return true;
}

void IRStorage::clearAll() {
    for (int i = 0; i < MAX_SIGNALS; i++) {
        signals[i].isValid = false;
    }
    signal_count = 0;
    saveToEEPROM();
    Serial.println("[Storage] 已清空所有信号");
}

IRSignal* IRStorage::getSignal(int id) {
    int index = id - 1;
    if (!isValidId(id)) {
        return nullptr;
    }
    return &signals[index];
}

int IRStorage::getSignalCount() {
    return signal_count;
}

bool IRStorage::isValidId(int id) {
    int index = id - 1;
    return (index >= 0 && index < MAX_SIGNALS && signals[index].isValid);
}

void IRStorage::listAllSignals() {
    Serial.printf("[Storage] 已存储信号列表 (%d/%d):\n", signal_count, MAX_SIGNALS);
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    if (signal_count == 0) {
        Serial.println("  (无信号)");
        return;
    }
    
    for (int i = 0; i < MAX_SIGNALS; i++) {
        if (signals[i].isValid) {
            Serial.printf("  ID:%2d | %-15s | %s | 0x%08X | %2d位\n", 
                         i + 1, 
                         signals[i].name,
                         typeToString(signals[i].protocol, false).c_str(),
                         (uint32_t)signals[i].value,
                         signals[i].bits);
        }
    }
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}

void IRStorage::printSignalInfo(int id) {
    IRSignal* signal = getSignal(id);
    if (!signal) {
        Serial.printf("[Storage] 无效的信号ID: %d\n", id);
        return;
    }
    
    Serial.printf("[Storage] 信号ID %d 详细信息:\n", id);
    Serial.printf("  名称: %s\n", signal->name);
    Serial.printf("  协议: %s\n", typeToString(signal->protocol, false).c_str());
    Serial.printf("  数值: 0x%08X\n", (uint32_t)signal->value);
    Serial.printf("  位数: %d\n", signal->bits);
    Serial.printf("  原始长度: %d\n", signal->rawLength);
    Serial.printf("  学习时间: %lu\n", signal->timestamp);
}

void IRStorage::printRawData(int id) {
    IRSignal* signal = getSignal(id);
    if (!signal) {
        Serial.printf("[Storage] 无效的信号ID: %d\n", id);
        return;
    }
    
    Serial.printf("[Storage] 信号ID %d 原始数据:\n", id);
    Serial.print("  数据: ");
    for (int i = 0; i < signal->rawLength; i++) {
        Serial.printf("%d", signal->rawData[i]);
        if (i < signal->rawLength - 1) Serial.print(",");
        if ((i + 1) % 16 == 0) Serial.print("\n        ");
    }
    Serial.println();
}

bool IRStorage::setSignalName(int id, const char* name) {
    IRSignal* signal = getSignal(id);
    if (!signal || !name) {
        return false;
    }
    
    strncpy(signal->name, name, 31);
    signal->name[31] = '\0';
    saveToEEPROM();
    
    Serial.printf("[Storage] 信号ID %d 名称已更新为: %s\n", id, name);
    return true;
}

int IRStorage::getUsedSlots() {
    return signal_count;
}

int IRStorage::getFreeSlots() {
    return MAX_SIGNALS - signal_count;
}

size_t IRStorage::getUsedMemory() {
    return sizeof(IRSignal) * signal_count + 2;  // +2 for magic and count
}
