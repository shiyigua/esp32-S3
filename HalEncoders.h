#pragma once

#include "Config.h"

class HalEncoders {
public:
    HalEncoders();
    
    // 初始化硬件 (GPIO, SPI)
    void begin();

    // 构建 SPI 命令帧，包括设置读写位并加入偶校验结果
    uint16_t build_command_frame(uint16_t address, bool is_read);

    // // 偶校验计算
    // uint8_t parityCheck(uint8_t *entry_data, uint16_t entry_len);

    // 执行一次完整的 21 轴读取 (包含 MUX 切换流程)
    // 注意：此函数耗时较长，通常在 Task 中调用
    void readAll();

    // 执行一次完整的 21 轴检查 (包含 MUX 切换流程)
    // 注意：此函数耗时较长，通常在 Task 中调用
    void checkAll();


    // 线程安全地获取当前数据副本
    EncoderData getData();

private:
    // 分组定义: 5组, 数量分别为 4,4,4,4,5
    const int group_sizes_[5] = {4, 4, 4, 4, 5};
    
    // 内部数据缓存
    EncoderData _data;
    SemaphoreHandle_t _mutex; // 互斥锁，保证数据完整性

    // SPI 句柄
    spi_device_handle_t _spi;

    // 辅助函数
    void setupSPI();
    void setMux(uint8_t channel);
    





    // ==========================================
    // 3. 寄存器地址
    // ==========================================
    const uint16_t AS5047P_REG_NOP         = 0x0000;    //空操作寄存器 (No Operation)。读取此寄存器相当于发送空指令，通常用于在不执行任何操作的情况下获取上一帧的数据。
    const uint16_t AS5047P_REG_ERRFL       = 0x0001;    //错误标志寄存器 (Error Flag)。存储通讯或硬件错误状态。读取后会自动清零。包含位：<br>- Bit 0: FRERR (帧错误)<br>- Bit 1: INVCOMM (无效命令)<br>- Bit 2: PARERR (奇偶校验错误
    const uint16_t AS5047P_REG_PROG        = 0x0003;    //编程寄存器 (Programming)。用于 OTP (一次性可编程存储器) 的编程操作。
    const uint16_t AS5047P_REG_DIAAGC      = 0x3FFC;    //诊断与自动增益控制 (Diagnostic and AGC)。<br>包含磁场强度状态标志（磁场过强/过弱）和当前 AGC 增益值，用于判断磁铁安装距离是否合适。
    const uint16_t AS5047P_REG_MAG         = 0x3FFD;    //CORDIC 幅值 (CORDIC Magnitude)。内部 CORDIC 算法计算出的磁场矢量幅值，反映当前检测到的磁场强度数据。
    const uint16_t AS5047P_REG_ANGLEUNC    = 0x3FFE;    //未补偿角度 (Angle Uncompensated)。<br>原始测量的 14-bit 角度值，不包含动态角度误差补偿 (DAEC™)。
    const uint16_t AS5047P_REG_ANGLECOM    = 0x3FFF;    //补偿后角度 (Angle Compensated)。<br>经过动态角度误差补偿 (DAEC™) 处理后的 14-bit 角度值。这是读取电机位置时最常用的寄存器。


};

// 全局实例声明
extern HalEncoders encoders;