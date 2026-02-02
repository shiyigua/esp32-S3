// #include "Calibration.h"

// CalibrationManager calibManager;

// void CalibrationManager::begin() {
//     // 1. 打开 NVS 命名空间，false 表示读写模式 (Read-Write)
//     _prefs.begin(_namespace, false);

//     // 2. 检查是否已经存在保存的校准数据
//     if (_prefs.isKey(_key_offsets)) {
//         // 从 Flash 读取字节流到内存数组
//         size_t len = _prefs.getBytes(_key_offsets, offsets, sizeof(offsets));
        
//         // 校验长度是否匹配（防止固件升级后电机数量变化导致错乱）
//         if (len != sizeof(offsets)) {
//             Serial.println("[Calib] Size mismatch! Resetting offsets to 0.");
//             memset(offsets, 0, sizeof(offsets));
//         } else {
//             Serial.println("[Calib] Offsets loaded from Flash.");
//         }
//     } else {
//         // 如果是第一次运行，没有数据，则全部初始化为 0
//         Serial.println("[Calib] No saved data found. Using default 0.");
//         memset(offsets, 0, sizeof(offsets));
//     }
// }

// void CalibrationManager::saveCurrentAsZero(const uint16_t* currentRawAngles) {
//     if (currentRawAngles == nullptr) return;

//     // 1. 更新内存副本
//     memcpy(offsets, currentRawAngles, sizeof(offsets));

//     // 2. 写入 Flash (这对操作会稍微耗时，不要在中断中调用)
//     size_t written = _prefs.putBytes(_key_offsets, offsets, sizeof(offsets));

//     if (written == sizeof(offsets)) {
//         Serial.println("[Calib] New Zero points Saved successfully!");
//     } else {
//         Serial.println("[Calib] Error: Failed to save to Flash!");
//     }
// }

// uint16_t CalibrationManager::getCalibratedAngle(int id, uint16_t rawValue) {
//     if (id < 0 || id >= ENCODER_TOTAL_NUM) return 0;
    
//     // AS5047P 是 14 位精度 (0 ~ 16383)
//     // 计算公式：(当前值 - 零点值 + 周期) % 周期
//     // 这样可以处理 0 点附近的环绕问题（例如：当前 10，零点 16300）
    
//     int32_t val = (int32_t)rawValue;
//     int32_t off = (int32_t)offsets[id];
    
//     int32_t diff = val - off;
    
//     // 归一化到 0 ~ 16383
//     if (diff < 0) {
//         diff += 16384; 
//     }
    
//     // 再次确保掩码安全
//     return (uint16_t)(diff & 0x3FFF);
// }

// void CalibrationManager::clearStorage() {
//     _prefs.clear();
//     memset(offsets, 0, sizeof(offsets));
//     Serial.println("[Calib] Storage Cleared!");
// }
#include "Calibration.h"

CalibrationManager calibManager;

CalibrationManager::CalibrationManager() {
    // 构造函数初始化，默认偏移量全为 0
    memset(offsets, 0, sizeof(offsets));
}

void CalibrationManager::begin() {
    _prefs.begin(_namespace, true); // true = 只读模式打开尝试读取
    size_t len = _prefs.getBytesLength(_key_offsets);
    
    if (len == sizeof(offsets)) {
        _prefs.getBytes(_key_offsets, offsets, sizeof(offsets));
        Serial.println("[Calib] Offsets loaded from NVS.");
    } else {
        Serial.println("[Calib] No valid offsets found. Using default 0.");
        memset(offsets, 0, sizeof(offsets));
    }
    _prefs.end();
}

void CalibrationManager::saveCurrentAsZero(const uint16_t* currentRawAngles) {
    if (currentRawAngles == nullptr) return;

    // 1. 更新内存副本
    memcpy(offsets, currentRawAngles, sizeof(offsets));

    // 2. 写入 Flash (切换为读写模式)
    _prefs.begin(_namespace, false); 
    size_t written = _prefs.putBytes(_key_offsets, offsets, sizeof(offsets));
    _prefs.end();

    if (written == sizeof(offsets)) {
        Serial.println("[Calib] New Zero points Saved successfully!");
    } else {
        Serial.println("[Calib] Error: Failed to save to Flash!");
    }
}

// =========================================================
// [修改重点] 批量转换函数
// =========================================================
void CalibrationManager::calibrateAll(const uint16_t* rawInput, uint16_t* calibratedOutput) {
    if (!rawInput || !calibratedOutput) return;

    for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
        // 1. 再次确保输入数据被掩码 (双重保险)
        uint16_t raw = rawInput[i] & 0x3FFF; 
        uint16_t off = offsets[i] & 0x3FFF;

        // 2. 核心公式：自动环绕减法
        // 解释：(raw - off) 在无符号运算下会自动回绕
        // 例如：10 - 20 = 65526 (-10的补码)
        // 65526 & 0x3FFF = 16374 (正确的环绕值)
        calibratedOutput[i] = (raw - off) & 0x3FFF;
    }
}
// void CalibrationManager::calibrateAll(const uint16_t* rawInput, uint16_t* calibratedOutput) {
//     if (rawInput == nullptr || calibratedOutput == nullptr) return;

//     // 添加调试打印（只打印第一个关节，避免刷屏），用于检查输入是否正常
//     // static uint32_t lastDebugTime = 0;
//     // if (millis() - lastDebugTime > 2000) {
//     //    Serial.printf("[Calib Debug] ID[0] Raw:%d Offset:%d\n", rawInput[0], offsets[0]);
//     //    lastDebugTime = millis();
//     // }

//     for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
//         // 1. 获取原始值和偏移值
//         uint16_t raw = rawInput[i];
//         uint16_t off = offsets[i];

//         // 2. 核心算法：利用无符号整数溢出特性 + 掩码截断
//         // 原理：
//         // 如果 raw=10, off=20 -> 10-20 = -10
//         // 在 uint16_t 中，-10 表现为 65526 (0xFFF6)
//         // 0xFFF6 & 0x3FFF (14位掩码) = 0x3FF6 = 16374
//         // 验证：16384 - 10 = 16374 (正确环绕)
        
//         uint16_t val = (raw - off) & ENCODER_RESOLUTION_BIT;
        
//         // 3. 赋值输出
//         calibratedOutput[i] = val;
//     }
// }
// void CalibrationManager::calibrateAll(const uint16_t* rawInput, uint16_t* calibratedOutput) {
//     if (rawInput == nullptr || calibratedOutput == nullptr) return;

//     for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
//         // 1. 读取原始值和对应的零点偏移
//         int32_t raw = (int32_t)rawInput[i];
//         int32_t off = (int32_t)offsets[i];

//         // 2. 计算差值 (Raw - Zero)
//         int32_t diff = raw - off;

//         // 3. 处理环绕 (Wrap-around)
//         // 例如：当前值 10，零点 16300 -> diff = -16290
//         // +16384 -> 94 (实际对应的正角度)
//         if (diff < 0) {
//             diff += ENCODER_PERIOD;
//         } 
//         // 这里的 else if 处理 diff > 16384 的情况通常不需要，
//         // 因为 AS5047P 原始值最大也就是 16383，除非 offsets 坏了。
//         // 但为了保险可以用位与操作屏蔽高位。

//         // 4. 输出结果 (确保限制在 0~16383)
//         calibratedOutput[i] = (uint16_t)(diff & ENCODER_RESOLUTION_BIT);
//     }
// }

void CalibrationManager::clearStorage() {
    _prefs.begin(_namespace, false);
    _prefs.clear();
    _prefs.end();
    memset(offsets, 0, sizeof(offsets));
    Serial.println("[Calib] Storage cleared!");
}