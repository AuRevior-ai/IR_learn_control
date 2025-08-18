#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

// 红外接收器类
class IRReceiver {
private:
    IRrecv* irrecv;
    decode_results results;
    uint8_t receive_pin;
    bool is_learning;
    unsigned long last_receive_time;
    
public:
    IRReceiver(uint8_t pin);
    ~IRReceiver();
    
    bool begin();
    bool isAvailable();
    bool decode();
    void startLearning();
    void stopLearning();
    bool isLearning();
    
    // 获取接收到的信号数据
    uint32_t getValue();
    uint16_t getBits();
    decode_type_t getProtocol();
    String getProtocolName();
    uint16_t* getRawData();
    uint16_t getRawLength();
    
    // 打印信号信息
    void printResult();
    void printAdvancedResult(); // 新增：高级结果显示
    String getResultString();
    
    void reset();
};

#endif
