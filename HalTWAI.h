// #ifndef HAL_TWAI_H
// #define HAL_TWAI_H

// #include <Arduino.h>
// #include "driver/twai.h"
// #include "Config.h"

// // 【新增】错误状态帧ID
// #define CAN_ID_ERROR_STATUS 0x1F0

// class HalTWAI {
// public:
//     // 原有接口 (保持不变)
//     void begin();
//     void sendEncoderAngles(const uint16_t* angles);
//     void sendTactileData(const uint8_t* data, int len);
//     bool receive(twai_message_t& msg);
    
//     // 【新增】发送错误状态
//     void sendErrorStatus(const EncoderData& data);
    
// private:
//     bool sendFrame(uint32_t id, const uint8_t* data, uint8_t len);
// };

// extern HalTWAI halTwai;

// #endif


#pragma once
#include "Config.h"

// 【新增】错误状态帧ID
#define CAN_ID_ERROR_STATUS 0x1F0

class HalTWAI {
public:
    HalTWAI();
    bool begin();
    
    // --- 发送函数声明 ---
    // 发送高频编码器数据
    void sendEncoderData(const EncoderData& encData);
    
    // 发送触觉合力概览
    void sendTactileSummary(const TactileData& tacData);
    
    // 发送详细触觉数据 (如果需要)
    void sendTactileFullDump(const TactileData& tacData);
    
    void sendErrorStatus(const EncoderData& data);

    // 【新增 2023-10】发送校准完成信号给 P4
    // 参数 success: true表示成功，false表示失败
    bool sendCalibrationAck(bool success); 
    
    // --- 接收函数声明 ---
    // 注意：统一为带参数版本，与 .cpp 保持一致
    bool receiveMonitor(RemoteCommand* outCmd);

    // 【新增】维护总线状态，返回 true 表示总线正常，false 表示正在恢复中
    bool maintain();

private:
    void sendFrame(uint32_t id, const uint8_t* data, uint8_t len);
    bool _is_initialized;
};

extern HalTWAI twaiBus;