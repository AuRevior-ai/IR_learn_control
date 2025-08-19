#ifndef IR_TRANSMITTER_H
#define IR_TRANSMITTER_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>
#include <driver/rmt.h>
#include <soc/rmt_reg.h>
#include <esp32-hal-rmt.h>

// RMT硬件发射器类 - 专门用于UNKNOWN协议的稳定发射
class RMTTransmitter {
private:
    rmt_channel_t channel;
    uint8_t pin;
    bool initialized;
    
    // 将微秒时间转换为RMT ticks
    uint32_t usToTicks(uint32_t us);
    
public:
    RMTTransmitter(uint8_t pin, rmt_channel_t ch = RMT_CHANNEL_0);
    ~RMTTransmitter();
    
    bool begin();
    bool sendRawData(uint16_t* rawData, uint16_t length, uint16_t freq = 38);
    void end();
};

// 红外发射器类
class IRTransmitter {
private:
    IRsend* irsend;
    RMTTransmitter* rmt_transmitter;
    uint8_t send_pin;
    bool is_sending;
    bool use_rmt_for_raw;
    
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
    
    // 新增：持续验证模式（每0.5秒发送一次，持续10秒）
    bool continuousVerifySignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                               uint16_t* rawData, uint16_t rawLength);
    
    // RMT硬件发射器控制
    bool enableRMT(bool enable = true);
    bool isRMTEnabled() const;
    
    // 测试函数
    bool testTransmitter();
    void testGPIO4();  // 新增：直接测试GPIO4输出
    
    // 状态查询
    bool isSending();
    
    // 设置载波频率
    void setFrequency(uint16_t freq);
};

#endif
