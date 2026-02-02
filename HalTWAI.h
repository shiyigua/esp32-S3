#pragma once
#include "Config.h"

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