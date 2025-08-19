#include "ir_transmitter.h"

// ============== RMTTransmitter 实现 ==============

RMTTransmitter::RMTTransmitter(uint8_t pin, rmt_channel_t ch) : pin(pin), channel(ch), initialized(false) {
}

RMTTransmitter::~RMTTransmitter() {
    if (initialized) {
        end();
    }
}

uint32_t RMTTransmitter::usToTicks(uint32_t us) {
    // ESP32 RMT时钟分频器设置为80 (80MHz / 80 = 1MHz, 1tick = 1us)
    return us;
}

bool RMTTransmitter::begin() {
    if (initialized) return true;
    
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = channel,
        .gpio_num = (gpio_num_t)pin,
        .clk_div = 80,  // 80MHz / 80 = 1MHz (1 tick = 1μs)
        .mem_block_num = 2,
        .flags = 0,
        .tx_config = {
            .carrier_freq_hz = 38000,  // 38kHz载波
            .carrier_level = RMT_CARRIER_LEVEL_HIGH,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = 33,  // 33%占空比
            .carrier_en = true,
            .loop_en = false,
            .idle_output_en = true
        }
    };
    
    esp_err_t ret = rmt_config(&config);
    if (ret != ESP_OK) {
        Serial.printf("[RMT] 配置失败: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    ret = rmt_driver_install(channel, 0, 0);
    if (ret != ESP_OK) {
        Serial.printf("[RMT] 驱动安装失败: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    initialized = true;
    Serial.printf("[RMT] 初始化成功，通道: %d, 引脚: GPIO%d\n", channel, pin);
    return true;
}

bool RMTTransmitter::sendRawData(uint16_t* rawData, uint16_t length, uint16_t freq) {
    if (!initialized || !rawData || length == 0) {
        return false;
    }
    
    Serial.printf("[RMT] 🚀 准备发射，原始长度: %d, 频率: %dkHz\n", length, freq);
    
    // 动态分配RMT数据项，避免栈溢出
    rmt_item32_t* rmt_items = (rmt_item32_t*)malloc(sizeof(rmt_item32_t) * (length / 2 + 2));
    if (!rmt_items) {
        Serial.println("[RMT] ❌ 内存分配失败");
        return false;
    }
    
    size_t rmt_size = 0;
    
    // 转换原始数据为RMT格式 (配对处理：高电平+低电平)
    for (int i = 0; i < length - 1; i += 2) {
        if (rmt_size >= (length / 2 + 1)) break;
        
        uint32_t high_time = usToTicks(rawData[i]);
        uint32_t low_time = usToTicks(rawData[i + 1]);
        
        // RMT限制：每个duration最大32767 ticks
        if (high_time > 32767) high_time = 32767;
        if (low_time > 32767) low_time = 32767;
        
        // 确保最小时间值，避免过短的脉冲
        if (high_time == 0) high_time = 1;
        if (low_time == 0) low_time = 1;
        
        // ✨ 改进：VS1838B兼容性调整
        // VS1838B对时序比较敏感，适当延长短脉冲
        if (high_time < 50) high_time = (uint32_t)(high_time * 1.2);  
        if (low_time < 50) low_time = (uint32_t)(low_time * 1.2);
        
        // 对于非常短的脉冲，设置最小值
        if (high_time < 10) high_time = 10;
        if (low_time < 10) low_time = 10;
        
        rmt_items[rmt_size].level0 = 1;
        rmt_items[rmt_size].duration0 = high_time;
        rmt_items[rmt_size].level1 = 0;
        rmt_items[rmt_size].duration1 = low_time;
        rmt_size++;
    }
    
    // 如果有奇数个元素，处理最后一个
    if (length % 2 == 1) {
        uint32_t final_time = usToTicks(rawData[length - 1]);
        if (final_time > 32767) final_time = 32767;
        if (final_time == 0) final_time = 1;
        
        rmt_items[rmt_size].level0 = 1;
        rmt_items[rmt_size].duration0 = final_time;
        rmt_items[rmt_size].level1 = 0;
        rmt_items[rmt_size].duration1 = 0;  // 结束标记
        rmt_size++;
    }
    
    // ✨ 新增：添加强化结束信号
    rmt_items[rmt_size].level0 = 0;
    rmt_items[rmt_size].duration0 = 1000;  // 1ms低电平确保信号结束
    rmt_items[rmt_size].level1 = 0;
    rmt_items[rmt_size].duration1 = 0;
    rmt_size++;
    
    Serial.printf("[RMT] 📊 转换完成: %d项RMT数据\n", rmt_size);
    
    // 设置载波频率
    if (freq != 38) {
        rmt_config_t config = {
            .rmt_mode = RMT_MODE_TX,
            .channel = channel,
            .gpio_num = (gpio_num_t)pin,
            .clk_div = 80,
            .mem_block_num = 2,
            .flags = 0,
            .tx_config = {
                .carrier_freq_hz = static_cast<uint32_t>(freq) * 1000U,
                .carrier_level = RMT_CARRIER_LEVEL_HIGH,
                .idle_level = RMT_IDLE_LEVEL_LOW,
                .carrier_duty_percent = 33,
                .carrier_en = true,
                .loop_en = false,
                .idle_output_en = true
            }
        };
        rmt_config(&config);
    }
    
    Serial.printf("[RMT] 发射信号，数据长度: %d -> %d项, 频率: %dkHz\n", length, rmt_size, freq);
    
    // ✨ 新增：多重发射增强稳定性
    bool success = false;
    for (int attempt = 1; attempt <= 2; attempt++) {
        Serial.printf("[RMT] 📡 第 %d/2 次发射尝试\n", attempt);
        
        // 发射数据
        esp_err_t ret = rmt_write_items(channel, rmt_items, rmt_size, true);
        
        if (ret == ESP_OK) {
            // 等待发射完成
            ret = rmt_wait_tx_done(channel, 1000 / portTICK_PERIOD_MS);  // 1秒超时
            
            if (ret == ESP_OK) {
                Serial.printf("[RMT] ✅ 第 %d 次发射成功\n", attempt);
                success = true;
                break;
            } else {
                Serial.printf("[RMT] ⚠️ 第 %d 次发射等待超时: %s\n", attempt, esp_err_to_name(ret));
            }
        } else {
            Serial.printf("[RMT] ❌ 第 %d 次发射失败: %s\n", attempt, esp_err_to_name(ret));
        }
        
        // 发射间隔
        if (attempt < 2) {
            delay(10);
        }
    }
    
    free(rmt_items);
    
    if (success) {
        Serial.println("[RMT] ✅ 发射完成");
        return true;
    } else {
        Serial.println("[RMT] ❌ 所有发射尝试均失败");
        return false;
    }
}

void RMTTransmitter::end() {
    if (initialized) {
        rmt_driver_uninstall(channel);
        initialized = false;
        Serial.println("[RMT] 驱动卸载完成");
    }
}

// ============== IRTransmitter 实现 ==============

IRTransmitter::IRTransmitter(uint8_t pin) {
    send_pin = pin;
    irsend = new IRsend(pin);
    rmt_transmitter = new RMTTransmitter(pin, RMT_CHANNEL_0);
    is_sending = false;
    use_rmt_for_raw = true;  // 默认启用RMT用于原始数据发射
}

IRTransmitter::~IRTransmitter() {
    if (irsend) {
        delete irsend;
    }
    if (rmt_transmitter) {
        delete rmt_transmitter;
    }
}

bool IRTransmitter::begin() {
    if (!irsend) return false;
    
    // 确保IRsend优先初始化
    irsend->begin();
    Serial.printf("[IR_TX] ✅ IRsend软件发射器初始化完成，引脚: GPIO%d\n", send_pin);
    
    // 初始化RMT发射器（但不影响IRsend）
    if (rmt_transmitter) {
        if (rmt_transmitter->begin()) {
            Serial.println("[IR_TX] ✅ RMT硬件发射器初始化成功");
        } else {
            Serial.println("[IR_TX] ⚠️ RMT硬件发射器初始化失败，将仅使用软件发射");
            use_rmt_for_raw = false;
        }
    } else {
        Serial.println("[IR_TX] ⚠️ RMT硬件发射器未创建，将仅使用软件发射");
        use_rmt_for_raw = false;
    }
    
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
    if (!rawData) return false;
    
    is_sending = true;
    Serial.printf("[IR_TX] 发射原始数据，长度: %d, 频率: %dkHz\n", length, freq);
    
    bool success = false;
    
    // 优先使用RMT硬件发射器发射原始数据（更稳定）
    if (use_rmt_for_raw && rmt_transmitter) {
        Serial.println("[IR_TX] 📡 使用RMT硬件发射器");
        success = rmt_transmitter->sendRawData(rawData, length, freq);
        
        if (!success) {
            Serial.println("[IR_TX] ⚠️ RMT发射失败，切换到软件发射");
        }
    }
    
    // 如果RMT发射失败或未启用，使用IRremoteESP8266软件发射
    if (!success && irsend) {
        Serial.println("[IR_TX] 📡 使用软件发射器");
        irsend->sendRaw(rawData, length, freq);
        success = true;
    }
    
    is_sending = false;
    return success;
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

// 带原始数据的发射函数 - 针对UNKNOWN协议优化
bool IRTransmitter::sendSignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                               uint16_t* rawData, uint16_t rawLength, uint16_t repeat) {
    
    // 对于UNKNOWN协议，优先使用原始数据发射
    if (protocol == UNKNOWN && rawData && rawLength > 0) {
        Serial.printf("[IR_TX] 🎯 检测到UNKNOWN协议\n");
        Serial.printf("[IR_TX] 📋 信号信息: 值=0x%08X, 位数=%d, 原始长度=%d\n", data, bits, rawLength);
        
        is_sending = true;
        bool success = false;
        
        // 根据RMT状态选择发射方式
        if (use_rmt_for_raw && rmt_transmitter) {
            Serial.println("[IR_TX] 📡 使用RMT硬件发射器");
            
            for (int attempt = 0; attempt <= repeat; attempt++) {
                Serial.printf("[IR_TX] 🔄 RMT发射第 %d/%d 次\n", attempt + 1, repeat + 1);
                
                delay(10);  // 发射前短暂延时
                success = rmt_transmitter->sendRawData(rawData, rawLength, 38);
                
                if (success) {
                    Serial.printf("[IR_TX] ✅ 第 %d 次RMT发射成功\n", attempt + 1);
                    break;
                } else {
                    Serial.printf("[IR_TX] ❌ 第 %d 次RMT发射失败\n", attempt + 1);
                }
                
                if (attempt < repeat) delay(100);
            }
        } else {
            Serial.println("[IR_TX] 📡 使用软件发射器");
            
            for (int attempt = 0; attempt <= repeat; attempt++) {
                Serial.printf("[IR_TX] 🔄 软件发射第 %d/%d 次\n", attempt + 1, repeat + 1);
                
                delay(10);  // 发射前短暂延时
                
                if (irsend) {
                    irsend->sendRaw(rawData, rawLength, 38);
                    success = true;
                    Serial.printf("[IR_TX] ✅ 第 %d 次软件发射完成\n", attempt + 1);
                } else {
                    Serial.printf("[IR_TX] ❌ 第 %d 次软件发射失败，IRsend未初始化\n", attempt + 1);
                }
                
                if (attempt < repeat) delay(100);
            }
        }
        
        is_sending = false;
        
        if (success) {
            Serial.println("[IR_TX] ✅ UNKNOWN协议发射完成");
        } else {
            Serial.println("[IR_TX] ❌ UNKNOWN协议发射失败");
        }
        
        return success;
    }
    
    // 对于已知协议，首先尝试协议特定方法
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
        
        bool success = false;
        for (int attempt = 0; attempt <= repeat; attempt++) {
            delay(10);
            success = sendRaw(rawData, rawLength, frequency);
            if (attempt < repeat) {
                delay(100);
            }
        }
        
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

// 新增：持续验证模式 - 每0.5秒发送一次，持续10秒，同时监控接收
bool IRTransmitter::continuousVerifySignal(decode_type_t protocol, uint32_t data, uint16_t bits, 
                                          uint16_t* rawData, uint16_t rawLength) {
    Serial.println("🔄 ========== 持续验证模式 ==========");
    Serial.printf("📋 信号信息: 协议=%s, 值=0x%08X, 位数=%d\n", 
                 typeToString(protocol, false).c_str(), data, bits);
    Serial.println("⏱️ 测试时长: 10秒，发射间隔: 0.5秒");
    Serial.println("📡 同时监控接收器实时反应...");
    Serial.println("====================================");
    
    const unsigned long TEST_DURATION = 10000;  // 10秒
    const unsigned long SEND_INTERVAL = 500;    // 0.5秒
    const unsigned long RECEIVE_TIMEOUT = 300;  // 接收超时300ms
    
    unsigned long startTime = millis();
    unsigned long lastSendTime = 0;
    int sendCount = 0;
    int sendSuccessCount = 0;
    int receiveSuccessCount = 0;
    int receiveMatchCount = 0;
    
    while (millis() - startTime < TEST_DURATION) {
        unsigned long currentTime = millis();
        
        // 每0.5秒发送一次
        if (currentTime - lastSendTime >= SEND_INTERVAL) {
            sendCount++;
            unsigned long remainingTime = TEST_DURATION - (currentTime - startTime);
            
            Serial.printf("\n🚀 [%d] 第%d次发射 (剩余%.1fs)...\n", 
                         sendCount, sendCount, remainingTime / 1000.0);
            
            bool sendSuccess = false;
            
            // 根据协议类型选择最佳发射方式
            if (protocol == UNKNOWN && rawData && rawLength > 0) {
                // UNKNOWN协议优先使用RMT硬件发射
                if (use_rmt_for_raw && rmt_transmitter) {
                    sendSuccess = rmt_transmitter->sendRawData(rawData, rawLength, 38);
                } else {
                    sendSuccess = sendRaw(rawData, rawLength, 38);
                }
            } else {
                // 已知协议使用标准方法
                sendSuccess = sendSignal(protocol, data, bits, rawData, rawLength, 0);
            }
            
            if (sendSuccess) {
                sendSuccessCount++;
                Serial.printf("  📡 发射成功 [%d/%d]\n", sendSuccessCount, sendCount);
            } else {
                Serial.printf("  ❌ 发射失败 [%d/%d]\n", sendSuccessCount, sendCount);
            }
            
            lastSendTime = currentTime;
        }
        
        // 短暂延时，避免占用太多CPU
        delay(10);
    }
    
    // 计算成功率
    float sendSuccessRate = sendCount > 0 ? (float)sendSuccessCount / sendCount * 100 : 0;
    
    Serial.println("\n🏁 ========== 验证结果总结 ==========");
    Serial.printf("📊 总发射次数: %d\n", sendCount);
    Serial.printf("✅ 发射成功: %d\n", sendSuccessCount);
    Serial.printf("❌ 发射失败: %d\n", sendCount - sendSuccessCount);
    Serial.printf("📈 发射成功率: %.1f%%\n", sendSuccessRate);
    Serial.println("💡 注意: 请同时观察接收器是否实时接收到信号");
    
    if (sendSuccessRate >= 90) {
        Serial.println("🎯 优秀: 信号发射非常稳定");
    } else if (sendSuccessRate >= 80) {
        Serial.println("✅ 良好: 信号发射稳定性不错");
    } else if (sendSuccessRate >= 60) {
        Serial.println("⚠️ 一般: 信号发射稳定性有待改善");
    } else {
        Serial.println("❌ 差: 信号发射不稳定，建议重新学习");
    }
    
    Serial.println("=====================================");
    return sendSuccessRate >= 80;
}

// 新增：信号验证测试 - 更新版本
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
        
        bool success = false;
        
        // 根据协议选择最佳发射方式
        if (protocol == UNKNOWN && rawData && rawLength > 0) {
            // UNKNOWN协议使用RMT硬件发射
            if (use_rmt_for_raw && rmt_transmitter) {
                success = rmt_transmitter->sendRawData(rawData, rawLength, 38);
            } else {
                success = sendRaw(rawData, rawLength, 38);
            }
        } else {
            success = sendSignal(protocol, data, bits, rawData, rawLength, 1);
        }
        
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

void IRTransmitter::testGPIO4() {
    Serial.println("[IR_TX] 🔍 GPIO4直接输出测试");
    Serial.println("[IR_TX] 💡 请用万用表监控GPIO4电压变化");
    Serial.println("====================================");
    
    // 直接控制GPIO4，绕过所有发射器
    pinMode(send_pin, OUTPUT);
    
    for (int i = 1; i <= 10; i++) {
        Serial.printf("[IR_TX] 第 %d/10 次测试\n", i);
        
        digitalWrite(send_pin, HIGH);
        Serial.printf("[IR_TX] GPIO%d = HIGH (3.3V)\n", send_pin);
        delay(500);
        
        digitalWrite(send_pin, LOW);  
        Serial.printf("[IR_TX] GPIO%d = LOW (0V)\n", send_pin);
        delay(500);
    }
    
    Serial.println("====================================");
    Serial.println("[IR_TX] ✅ GPIO4直接测试完成");
    Serial.println("[IR_TX] 💡 如果万用表有电压变化，说明GPIO4工作正常");
    Serial.println("[IR_TX] 💡 如果没有变化，说明硬件连接问题");
    
    // 恢复IRsend配置
    if (irsend) {
        irsend->begin();
        Serial.printf("[IR_TX] 🔄 IRsend发射器已重新初始化，GPIO%d\n", send_pin);
    }
}

bool IRTransmitter::isSending() {
    return is_sending;
}

void IRTransmitter::setFrequency(uint16_t freq) {
    Serial.printf("[IR_TX] 设置载波频率: %dkHz\n", freq);
    // IRsend库会自动处理频率设置
}

bool IRTransmitter::enableRMT(bool enable) {
    if (!rmt_transmitter) {
        Serial.println("[IR_TX] RMT硬件发射器未初始化");
        return false;
    }

    if (enable) {
        if (!rmt_transmitter->begin()) {
            Serial.println("[IR_TX] RMT硬件发射器启用失败");
            use_rmt_for_raw = false;
            return false;
        }
        use_rmt_for_raw = true;
        Serial.println("[IR_TX] ✅ RMT硬件发射器已启用");
    } else {
        use_rmt_for_raw = false;
        // 重新初始化IRsend确保软件发射正常
        if (irsend) {
            irsend->begin();
            Serial.println("[IR_TX] ✅ RMT硬件发射器已禁用，IRsend软件发射器已重新初始化");
        } else {
            Serial.println("[IR_TX] RMT硬件发射器已禁用");
        }
    }

    return true;
}

bool IRTransmitter::isRMTEnabled() const {
    return use_rmt_for_raw;
}
