#ifndef IR_TRANSMITTER_H
#define IR_TRANSMITTER_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

// 红外发射器类
class IRTransmitter {
private:
    IRsend* irsend;
    uint8_t send_pin;
    bool is_sending;
    
public:
    IRTransmitter(uint8_t pin);
    ~IRTransmitter();
    
    bool begin();
    
    // 发射标准协议信号
    bool sendNEC(uint32_t data, uint16_t bits = 32, uint16_t repeat = 0);
    bool sendSony(uint32_t data, uint16_t bits = 12, uint16_t repeat = 0);
    bool sendRC5(uint32_t data, uint16_t bits = 12, uint16_t repeat = 0);
    
    // 发射原始数据
    bool sendRaw(uint16_t* rawData, uint16_t length, uint16_t freq = 38);
    
    // 通用发射函数
    bool sendSignal(decode_type_t protocol, uint32_t data, uint16_t bits, uint16_t repeat = 0);
    
    // 带原始数据的发射函数（用于UNKNOWN协议）
    bool sendSignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                   uint16_t* rawData, uint16_t rawLength, uint16_t repeat = 0);
    
    // 信号验证测试（连续发射用于稳定性测试）
    bool verifySignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                     uint16_t* rawData, uint16_t rawLength, uint16_t testCount = 5);
    
    // 测试函数
    bool testTransmitter();
    
    // 状态查询
    bool isSending();
    
    // 设置载波频率
    void setFrequency(uint16_t freq);
};

#endif
