// #include "HalTWAI.h"

// HalTWAI twaiBus;

// HalTWAI::HalTWAI() : _is_initialized(false) {}

// bool HalTWAI::begin() {
//     twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
//         (gpio_num_t)PIN_TWAI_TX, 
//         (gpio_num_t)PIN_TWAI_RX, 
//         TWAI_MODE_NORMAL
//     );
//     // 增加TX队列以及时容纳突发数据
//     g_config.tx_queue_len = 50; 
//     g_config.rx_queue_len = 10;
    
//     twai_timing_config_t t_config = CAN_BAUD_RATE;
//     twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

//     if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
//     if (twai_start() != ESP_OK) return false;

//     _is_initialized = true;
//     Serial.println("[TWAI] CAN Bus Started @ 1Mbps");
//     return true;
// }

// // 辅助：发送标准帧
// void HalTWAI::sendFrame(uint32_t id, const uint8_t* data, uint8_t len) {
//     if (!_is_initialized) return;
//     twai_message_t message;
//     message.identifier = id;
//     message.extd = 0;
//     message.data_length_code = len;
//     memcpy(message.data, data, len);
//     // 允许 1ms 阻塞等待，防止队列满丢包严重
//     twai_transmit(&message, pdMS_TO_TICKS(1)); 
// }

// // === 1. 发送编码器 (42 bytes -> 6 Frames) ===
// // 频率: 高 (跟随采集频率)
// void HalTWAI::sendEncoderData(const EncoderData& encData) {
//     uint8_t buffer[8];
//     int idx = 0;
    
//     // 前5帧
//     for(int i=0; i<5; i++) {
//         memcpy(buffer, &encData.rawAngle[idx], 8);
//         sendFrame(0x100 + i, buffer, 8);
//         idx += 4;
//     }
//     // 第6帧
//     memset(buffer, 0, 8);
//     memcpy(buffer, &encData.rawAngle[idx], 2);
//     sendFrame(0x105, buffer, 8);
// }

// // === 2. 发送触觉合力 (快速预览) ===
// // 仅发送 global force，数据量小，可高频发送
// // 5组 * (3+3+3) = 45 bytes -> 约 6~7 帧
// void HalTWAI::sendTactileSummary(const TactileData& tacData) {
//     uint8_t buffer[8];
//     uint8_t frameCount = 0;
//     uint8_t bufPtr = 0;

//     // 遍历所有组，提取合力
//     for(int g=0; g<TACTILE_GROUP_NUM; g++) {
//         const auto& grp = tacData.groups[g];
//         // 依次压入 A, B, C 的合力 (各3字节)
//         const uint8_t* ptrs[3] = {grp.sensor_A.global, grp.sensor_B.global, grp.sensor_C.global};
        
//         for(int s=0; s<3; s++) {
//             for(int k=0; k<3; k++) {
//                 buffer[bufPtr++] = ptrs[s][k];
//                 if(bufPtr == 8) {
//                     sendFrame(0x200 + frameCount++, buffer, 8);
//                     bufPtr = 0;
//                     memset(buffer, 0, 8);
//                 }
//             }
//         }
//     }
//     // 发送残余数据
//     if(bufPtr > 0) {
//         sendFrame(0x200 + frameCount, buffer, bufPtr);
//     }
// }

// // === 3. 发送触觉详细数据 (全量) ===
// // 数据量 > 1.5KB，需要在后台切片发送，不要阻塞主循环太久
// void HalTWAI::sendTactileFullDump(const TactileData& tacData) {
//     // 定义一个简单的流式协议：
//     // ID 0x300
//     // Byte 0: 包序号 (0~255, 循环)
//     // Byte 1: 组号 (0-4) + 传感器号(0-2) -> 高4位组，低4位传
//     // Byte 2-7: 6字节数据payload
    
//     static uint8_t pkgSeq = 0;
//     uint8_t buffer[8];
    
//     // 这里使用简化逻辑：直接按内存顺序 dump，接收端需要对齐
//     // 为了可靠性，建议仅在特定调试模式下开启此函数，或者分时复用
    
//     // 示例：仅发送第 0 组的详细数据用于测试 (避免此时卡死)
//     // 实际应用需配合流控
// }

// // 接收检查
// bool HalTWAI::receiveMonitor() {
//     twai_message_t message;
//     if (twai_receive(&message, 0) == ESP_OK) {
//         // 处理接收到的指令
//         return true;
//     }
//     return false;
// }


// //22222
// #include "HalTactile.h"
// #include <cstring>

// HalTactile tactile;

// HalTactile::HalTactile() {
//     _mutex = xSemaphoreCreateMutex();
// }

// void HalTactile::begin() {
//     // 初始化 SPI 引脚等逻辑...
//     // 这里的初始化代码可以保留你原来的
// }

// void HalTactile::update() {
//     // [注意] 这里的 SPI 读取代码需要根据新的 1575 字节数据结构重写
//     // 为了先让编译通过，我们只更新互斥锁保护的数据结构
    
//     if (xSemaphoreTake(_mutex, 10)) {
//         // 模拟数据更新，防止数据为空
//         // 实际上这里应该通过 SPI 读取 _data.groups...
        
//         // 示例：给第0组第A个传感器的合力赋个值证明它是活的
//         _data.groups[0].sensor_A.global[0]++; 
        
//         xSemaphoreGive(_mutex);
//     }
// }

// TactileData HalTactile::getData() {
//     TactileData ret;
//     // 线程安全地复制整个大结构体
//     if (xSemaphoreTake(_mutex, 10)) {
//         ret = _data;
//         xSemaphoreGive(_mutex);
//     }
//     // 注意：如果没有获取到锁，返回的 ret 内容未定义（垃圾值）
//     // 实际工程中可能需要处理这种情况
//     return ret;
// }

//3333
#include "HalTWAI.h"
#include <cstring>

// 全局实例
HalTWAI twaiBus;

HalTWAI::HalTWAI() : _is_initialized(false) {}

bool HalTWAI::begin() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)PIN_TWAI_TX,
        (gpio_num_t)PIN_TWAI_RX,
        TWAI_MODE_NORMAL
    );
    g_config.tx_queue_len = 50;
    g_config.rx_queue_len = 10;

    twai_timing_config_t t_config = CAN_BAUD_RATE;
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        return false;
    }
    if (twai_start() != ESP_OK) {
        return false;
    }

    _is_initialized = true;
    return true;
}

void HalTWAI::sendFrame(uint32_t id, const uint8_t* data, uint8_t len) {
    if (!_is_initialized) return;

    twai_message_t message;
    message.identifier = id;
    message.extd = 0;
    message.rtr = 0;
    message.data_length_code = len;
    memcpy(message.data, data, len);

    twai_transmit(&message, 0);
}

void HalTWAI::sendEncoderData(const EncoderData& encData) {
    uint8_t buffer[8];
    int idx = 0;
    // static int times;
    // 发送 21 个 uint16_t 角度值 (分 6 帧)
    for (int i = 0; i < 5; i++) {
        memcpy(buffer, &encData.rawAngle[idx], 8);
        sendFrame(0x100 + i, buffer, 8);
        idx += 4;
        // Serial.printf("第%d次can发送执行完毕\n",times);
        // times++;
    }
    // 最后一帧 (1 个角度)
    memset(buffer, 0, 8);
    memcpy(buffer, &encData.rawAngle[idx], 2);
    sendFrame(0x105, buffer, 8);
}

void HalTWAI::sendTactileSummary(const TactileData& tacData) {
    uint8_t buffer[8];
    uint8_t bufPtr = 0;
    uint32_t frameId = 0x200;

    for (int g = 0; g < TACTILE_GROUP_NUM; g++) {
        const TacGroup& grp = tacData.groups[g];
        const uint8_t* ptrs[3] = {
            grp.sensor_A.global,
            grp.sensor_B.global,
            grp.sensor_C.global
        };

        for (int s = 0; s < 3; s++) {
            for (int k = 0; k < 3; k++) {
                buffer[bufPtr++] = ptrs[s][k];
                if (bufPtr == 8) {
                    sendFrame(frameId++, buffer, 8);
                    bufPtr = 0;
                    memset(buffer, 0, 8);
                }
            }
        }
    }
    if (bufPtr > 0) {
        sendFrame(frameId, buffer, bufPtr);
    }
}

void HalTWAI::sendTactileFullDump(const TactileData& tacData) {
    // 占位，按需实现
}

bool HalTWAI::receiveMonitor(RemoteCommand* outCmd) {
    if (!_is_initialized) return false;

    twai_message_t message;
    if (twai_receive(&message, 0) == ESP_OK) {
        if (outCmd) {
            outCmd->cmd_type = message.data[0];
            outCmd->is_new = true;
        }
        return true;
    }
    return false;
}


bool HalTWAI::maintain() {
    twai_status_info_t status_info;
    
    // 获取当前驱动状态
    if (twai_get_status_info(&status_info) != ESP_OK) {
        return false;
    }

    // 1. 处理 Bus-Off (总线挂死)
    if (status_info.state == TWAI_STATE_BUS_OFF) {
        Serial.println("!!! [CAN ALERT] BUS-OFF Detected! Initiating Recovery... !!!");
        
        // 触发恢复流程 (这需要一定时间，取决于总线活动)
        // 注意：恢复期间不能发送/接收
        if (twai_initiate_recovery() != ESP_OK) {
            Serial.println("!!! [CAN FAIL] Recovery Initiation Failed");
        }
        return false;
    }

    // 2. 处理 Stopped 状态 (恢复完成后，驱动处于停止状态，需要重新 Start)
    if (status_info.state == TWAI_STATE_STOPPED) {
        Serial.println(">>> [CAN INFO] Bus Recovered. Restarting Driver...");
        
        if (twai_start() == ESP_OK) {
            Serial.println(">>> [CAN INFO] Driver Restarted Successfully.");
            // 可以在这里重置某些错误计数
        } else {
            Serial.println("!!! [CAN FAIL] Driver Start Failed");
        }
        return false; // 刚启动，本周期暂不处理数据
    }

    // 3. 检查接收队列溢出 (Rx Overrun)
    if (status_info.rx_overrun_count > 0) {
        Serial.printf("[CAN WARN] Rx Buffer Overrun: %d\n", status_info.rx_overrun_count);
        // 如果溢出严重，可能需要清空接收队列
    }

    // 4. 检查错误计数 (可选报警)
    if (status_info.tx_error_counter > 100 || status_info.rx_error_counter > 100) {
        Serial.printf("[CAN WARN] High Error Count! Tx:%d Rx:%d\n", 
                      status_info.tx_error_counter, status_info.rx_error_counter);
    }

    return (status_info.state == TWAI_STATE_RUNNING);
}