#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"     // 包含 ENCODER_TOTAL_NUM 定义
#include "HalEncoders.h" // 包含 EncoderData 定义

class CalibrationManager {
public:
    // 存储所有关节的零点偏移值
    uint16_t offsets[ENCODER_TOTAL_NUM]; 

    // 初始化：从 Flash 加载数据
    void begin();

    // 校准命令：将当前所有编码器的原始值保存为零点
    void saveCurrentAsZero(const uint16_t* currentRawAngles);

    // 获取校准后的角度 (Raw - Offset)
    uint16_t getCalibratedAngle(int id, uint16_t rawValue);

    // 仅用于调试：清除存储
    void clearStorage();

private:
    Preferences _prefs;
    const char* _namespace = "robot_cal";  // NVS 命名空间
    const char* _key_offsets = "offsets";  // 键名
};

// 声明全局实例
extern CalibrationManager calibManager;

#endif