#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"     // 包含 ENCODER_TOTAL_NUM 定义

// 定义分辨率掩码 (AS5047P: 14bit = 0x3FFF)
#define ENCODER_RESOLUTION_BIT 0x3FFF 
#define ENCODER_PERIOD        16384

class CalibrationManager {
public:
    // 存储所有关节的零点偏移值 (Public 方便调试，也可设为 Private)
    uint16_t offsets[ENCODER_TOTAL_NUM]; 

    CalibrationManager();
    
    // 初始化：从 Flash 加载数据
    void begin();

    // 1. [写] 校准命令：将当前所有编码器的原始值保存为零点
    //    currentRawAngles: 包含所有关节原始数值的数组
    void saveCurrentAsZero(const uint16_t* currentRawAngles);

    // 2. [读] 核心转换函数：一次性计算所有关节的校准角度
    //    rawInput:        输入的原始值数组 (源数据)
    //    calibratedOutput: 输出的校准后角度数组 (目标容器)
    void calibrateAll(const uint16_t* rawInput, uint16_t* calibratedOutput);

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