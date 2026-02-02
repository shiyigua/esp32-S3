#include "Calibration.h"

CalibrationManager calibManager;

void CalibrationManager::begin() {
    // 1. 打开 NVS 命名空间，false 表示读写模式 (Read-Write)
    _prefs.begin(_namespace, false);

    // 2. 检查是否已经存在保存的校准数据
    if (_prefs.isKey(_key_offsets)) {
        // 从 Flash 读取字节流到内存数组
        size_t len = _prefs.getBytes(_key_offsets, offsets, sizeof(offsets));
        
        // 校验长度是否匹配（防止固件升级后电机数量变化导致错乱）
        if (len != sizeof(offsets)) {
            Serial.println("[Calib] Size mismatch! Resetting offsets to 0.");
            memset(offsets, 0, sizeof(offsets));
        } else {
            Serial.println("[Calib] Offsets loaded from Flash.");
        }
    } else {
        // 如果是第一次运行，没有数据，则全部初始化为 0
        Serial.println("[Calib] No saved data found. Using default 0.");
        memset(offsets, 0, sizeof(offsets));
    }
}

void CalibrationManager::saveCurrentAsZero(const uint16_t* currentRawAngles) {
    if (currentRawAngles == nullptr) return;

    // 1. 更新内存副本
    memcpy(offsets, currentRawAngles, sizeof(offsets));

    // 2. 写入 Flash (这对操作会稍微耗时，不要在中断中调用)
    size_t written = _prefs.putBytes(_key_offsets, offsets, sizeof(offsets));

    if (written == sizeof(offsets)) {
        Serial.println("[Calib] New Zero points Saved successfully!");
    } else {
        Serial.println("[Calib] Error: Failed to save to Flash!");
    }
}

uint16_t CalibrationManager::getCalibratedAngle(int id, uint16_t rawValue) {
    if (id < 0 || id >= ENCODER_TOTAL_NUM) return 0;
    
    // AS5047P 是 14 位精度 (0 ~ 16383)
    // 计算公式：(当前值 - 零点值 + 周期) % 周期
    // 这样可以处理 0 点附近的环绕问题（例如：当前 10，零点 16300）
    
    int32_t val = (int32_t)rawValue;
    int32_t off = (int32_t)offsets[id];
    
    int32_t diff = val - off;
    
    // 归一化到 0 ~ 16383
    if (diff < 0) {
        diff += 16384; 
    }
    
    // 再次确保掩码安全
    return (uint16_t)(diff & 0x3FFF);
}

void CalibrationManager::clearStorage() {
    _prefs.clear();
    memset(offsets, 0, sizeof(offsets));
    Serial.println("[Calib] Storage Cleared!");
}