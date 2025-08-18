#ifndef IR_STORAGE_H
#define IR_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <IRremoteESP8266.h>

// 红外信号数据结构
struct IRSignal {
    bool isValid;                  // 信号是否有效
    decode_type_t protocol;        // 协议类型
    uint32_t value;               // 信号值
    uint16_t bits;                // 位数
    uint16_t rawLength;           // 原始数据长度
    uint16_t rawData[256];        // 原始数据(最大256个数据点)
    char name[32];                // 信号名称
    unsigned long timestamp;       // 学习时间戳
};

// 红外信号存储管理类
class IRStorage {
private:
    static const int MAX_SIGNALS = 20;      // 最大存储信号数量
    static const int EEPROM_SIZE = 4096;    // EEPROM大小
    static const int MAGIC_NUMBER = 0xAB;   // 魔数，用于验证数据有效性
    
    IRSignal signals[MAX_SIGNALS];
    int signal_count;
    
    void loadFromEEPROM();
    void saveToEEPROM();
    int findEmptySlot();
    
public:
    IRStorage();
    
    bool begin();
    
    // 信号管理
    int addSignal(decode_type_t protocol, uint32_t value, uint16_t bits, 
                  uint16_t* rawData, uint16_t rawLength, const char* name = nullptr);
    bool deleteSignal(int id);
    void clearAll();
    
    // 信号查询
    IRSignal* getSignal(int id);
    int getSignalCount();
    bool isValidId(int id);
    
    // 信号操作
    void listAllSignals();
    void printSignalInfo(int id);
    void printRawData(int id);
    
    // 设置信号名称
    bool setSignalName(int id, const char* name);
    
    // 统计信息
    int getUsedSlots();
    int getFreeSlots();
    size_t getUsedMemory();
};

#endif
