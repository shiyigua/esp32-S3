#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "driver/twai.h" // 确保包含 TWAI 定义


// ===========================================
// [System] 数据结构定义
// ===========================================
#define ENCODER_TOTAL_NUM 21
#define TACTILE_GROUP_NUM 5

// 编码器数据结构
struct EncoderData
{
    uint16_t rawAngle[ENCODER_TOTAL_NUM];         // 角度值 (0-16383)
    uint16_t errorFlags[ENCODER_TOTAL_NUM];       // 错误标志
    bool  parityCheckFlags[ENCODER_TOTAL_NUM]; // 偶校验标志
};

// 编码器检查数据结构
struct CheckData
{
    uint16_t rawData[ENCODER_TOTAL_NUM];         // 角度值 (0-16383)
    bool magHigh[ENCODER_TOTAL_NUM]; // 磁场过强标志。<br>1 = 磁铁距离芯片太近或磁性太强。<br>建议增大安装空隙。
    bool magLow[ENCODER_TOTAL_NUM];  // 磁场过弱标志。<br>1 = 磁铁距离芯片太远或磁性太弱。<br>建议减小安装空隙。
    bool cof[ENCODER_TOTAL_NUM];     // CORDIC 溢出。<br>1 = 内部计算溢出，通常伴随磁场异常，此时角度数据不可信。
    uint8_t agc[ENCODER_TOTAL_NUM];      // 自动增益值 (0~255) 。<br>反映芯片为了补偿磁场强度而调节的信号增益：<br>- 0: 磁场极强（增益最小）。<br>- 255: 磁场极弱（增益最大）。<br>- 理想值: 约 128 (通常在 50~200 之间为佳)。
};


// --- 触觉传感器数据结构 (根据你的描述修改) ---

// 1610 型号: 25点 * 3轴 + 3合力 = 75 + 3 = 78 Bytes
struct TacIndent1610 {
    uint8_t forces[25][3]; // 详细点阵力
    uint8_t global[3];     // 合力 Fx, Fy, Fz
};

// 2015 型号: 52点 * 3轴 + 3合力 = 156 + 3 = 159 Bytes
struct TacIndent2015 {
    uint8_t forces[52][3]; // 详细点阵力
    uint8_t global[3];     // 合力 Fx, Fy, Fz
};

// 单个模组包含的传感器
struct TacGroup {
    TacIndent1610 sensor_A; // 1610 #1
    TacIndent1610 sensor_B; // 1610 #2
    TacIndent2015 sensor_C; // 2015 #1
};

// 整个系统的触觉数据快照
struct TactileData {
    TacGroup groups[TACTILE_GROUP_NUM];
};

// --- 远程指令结构 ---
struct RemoteCommand {
    uint8_t cmd_type;
    float   value;
    bool    is_new;
};

// ===========================================
// [Hardware] 引脚定义 (ESP32-S3)
// ===========================================
// --- HSPI: 编码器 (AS5047P) ---
// 请根据您的电路图核对 GPIO
#define PIN_ENC_MISO    47
#define PIN_ENC_MOSI    38
#define PIN_ENC_SCLK    48
#define PIN_ENC_CS      7   // 公共片选

// MUX (74HC151) 用于切换 5 组传感器
#define PIN_MUX_A       2
#define PIN_MUX_B       4
#define PIN_MUX_C       5

// --- VSPI: 触觉传感器 ---
#define PIN_TAC_MISO    13
#define PIN_TAC_MOSI    11
#define PIN_TAC_SCLK    12
#define PIN_TAC_CS_A    7
#define PIN_TAC_CS_B    6
//#define PIN_TAC_CS_C    8  //引脚c被电阻一直上拉


// [Hardware] 引脚定义
// ===========================================
// TWAI (CAN)
#define PIN_TWAI_TX     9 
#define PIN_TWAI_RX     8
#define CAN_BAUD_RATE   TWAI_TIMING_CONFIG_1MBITS()

// MUX 控制引脚 (假设用于切换 5 组触觉)
#define PIN_MUX_A       2
#define PIN_MUX_B       4
#define PIN_MUX_C       5


// ===========================================
// [Parameter] 任务与通信参数
// ===========================================
#define SPI_FREQ_ENC    10000000 // 10MHz
#define SPI_FREQ_TAC    5000000  // 5MHz

// 任务优先级与核心
#define TASK_ENC_PRIO   10
#define TASK_ENC_CORE   0
#define TASK_TAC_PRIO   5
#define TASK_TAC_CORE   1

#endif // CONFIG_H