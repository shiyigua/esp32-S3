#include "HalEncoders.h"
#include <cstring>

HalEncoders encoders; // 实例化





/**
 * @brief 计算奇偶校验 (使用 ESP32 硬件指令优化)
 * @return 1 (奇数个1), 0 (偶数个1)
 * __builtin_parity 是 GCC 内置函数，编译为单条汇编指令，这里绝对不会卡死
 */
static inline uint8_t get_parity(uint16_t n) {
    return __builtin_parity(n); 
}



HalEncoders::HalEncoders() {
    memset(&_data, 0, sizeof(_data));
    _mutex = xSemaphoreCreateMutex();
}

void HalEncoders::begin() {
    // 1. 配置 MUX 和 CS GPIO
    pinMode(PIN_ENC_CS, OUTPUT);
    pinMode(PIN_MUX_A, OUTPUT);
    pinMode(PIN_MUX_B, OUTPUT);
    pinMode(PIN_MUX_C, OUTPUT);
    
    digitalWrite(PIN_ENC_CS, HIGH); // CS Idle

    // 2. 配置 SPI
    setupSPI();
}

void HalEncoders::setupSPI() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_ENC_MOSI;
    buscfg.miso_io_num = PIN_ENC_MISO;
    buscfg.sclk_io_num = PIN_ENC_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 32;

    // 使用 SPI2 (HSPI/FSPI)
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = SPI_FREQ_ENC;
    devcfg.mode = 1;         // AS5047P Mode 1
    devcfg.spics_io_num = -1; // 手动控制 CS
    devcfg.queue_size = 1;

    spi_bus_add_device(SPI2_HOST, &devcfg, &_spi);
}

void HalEncoders::setMux(uint8_t ch) {
    digitalWrite(PIN_MUX_A, (ch & 0x01));
    digitalWrite(PIN_MUX_B, (ch >> 1) & 0x01);
    digitalWrite(PIN_MUX_C, (ch >> 2) & 0x01);
    delayMicroseconds(1); // 稳定延时
}


/**
 * @brief 构建 SPI 命令帧，包括设置读写位并加入偶校验结果
 */
uint16_t HalEncoders::build_command_frame(uint16_t address, bool is_read) {
    // 1. 设置读/写位 (Bit 14)
    if (is_read) {
        address |= 0x4000;
    } else {
        address &= ~0x4000;
    }
    
    // 2. 计算低15位的奇偶性
    // 如果低15位里有奇数个1，__builtin_parity返回1
    // 为了使总共有偶数个1 (Even Parity)，如果当前是奇数，我们需要把Bit 15置1
    if (get_parity(address & 0x7FFF)) {
        address |= 0x8000; // Bit 15 置 1
    } else {
        address &= ~0x8000;
    }
    
    return address;
}

void HalEncoders::readAll() {
    // 临时缓冲区 (最大一组 5 个)
    uint16_t tx_buf[5];
    uint16_t rx_buf[5];
    spi_transaction_t t;
    
    // 构建读取角度命令 为寄存器地址添加奇偶校验和读写标识符
    uint16_t CMD_READ_ANGLE = HalEncoders::build_command_frame(AS5047P_REG_ANGLECOM, true); // Parity adjusted

    // 预备发送指令 (考虑大小端，ESP32是小端，SPI发高位在前，需翻转)
    // 0xFFFF 全是1，翻转后还是 0xFFFF，所以直接用即可，但为了通用性：
    uint16_t cmd = (CMD_READ_ANGLE >> 8) | (CMD_READ_ANGLE << 8);

    int global_idx = 0;

    //测试使用户；打印运行次数
    static int test_times;
    // 上锁，避免读取时被 getData 打断（尽管写入很快，安全第一）
    // 或者我们只在最后拷贝时上锁。这里为了简化逻辑，读写分离。
    // 在这里我们先读到局部变量，最后再上锁写入 _data。
    EncoderData tempDatas;
    
    for (int grp = 0; grp < 5; grp++) {
        int count = group_sizes_[grp];

        // 1. 填充 TX
        for(int k=0; k<count; k++) tx_buf[k] = cmd;

        // 2. 切换通道
        setMux(grp+3);

        // 3. SPI 传输
        memset(&t, 0, sizeof(t));
        t.length = count * 16;
        t.tx_buffer = tx_buf;
        t.rx_buffer = rx_buf;

        digitalWrite(PIN_ENC_CS, LOW);
        spi_device_polling_transmit(_spi, &t);
        digitalWrite(PIN_ENC_CS, HIGH);

        // 4. 解析数据 & 拓扑映射
        // AS5047P Daisy Chain: data[0] is from the LAST functionality device in chain
        for (int i = 0; i < count; i++) {
            uint16_t val = (rx_buf[i] << 8) | (rx_buf[i] >> 8); // 还原端序
            
            // 简单物理映射：假设数组按照物理连接顺序从前到后
            // rx_buf[0] 是链尾，rx_buf[count-1] 是链头
            int target_id = global_idx + (count - 1 - i);
            
            tempDatas.rawAngle[target_id] = val & 0x3FFF;
            tempDatas.errorFlags[target_id] = (val & 0x4000) ? 1 : 0;

            // Serial.print("Encoder ");
            // Serial.print(target_id);
            // Serial.print(": ");
            // Serial.println(tempDatas.rawAngle[target_id]);  

            // 检查校验位 (Bit 15)
            // 计算接收到的数据的校验 (包括数据位和错误标志位)
            uint16_t data_no_parity = val & 0x7FFF;
            uint8_t calc_parity = get_parity(data_no_parity);
            uint8_t recv_parity = (val >> 15) & 0x01;
            tempDatas.parityCheckFlags[target_id] = (calc_parity == recv_parity) ? 1 : 0;
        }
        global_idx += count;
    }
    // Serial.println(test_times); 
    test_times++; 
    // 更新全局数据 (线程安全)
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        memcpy(&_data, &tempDatas, sizeof(EncoderData));
        xSemaphoreGive(_mutex);
    }
}

void HalEncoders::checkAll() {
    // 临时缓冲区 (最大一组 5 个)
    uint16_t tx_buf[5];
    uint16_t rx_buf[5];
    spi_transaction_t t;
    
    // 构建读取角度命令 为寄存器地址添加奇偶校验和读写标识符
    uint16_t CMD_READ_ANGLE = HalEncoders::build_command_frame(AS5047P_REG_ANGLECOM, true); // Parity adjusted

    // 预备发送指令 (考虑大小端，ESP32是小端，SPI发高位在前，需翻转)
    // 0xFFFF 全是1，翻转后还是 0xFFFF，所以直接用即可，但为了通用性：
    uint16_t cmd = (CMD_READ_ANGLE >> 8) | (CMD_READ_ANGLE << 8);

    int global_idx = 0;

    // 上锁，避免读取时被 getData 打断（尽管写入很快，安全第一）
    // 或者我们只在最后拷贝时上锁。这里为了简化逻辑，读写分离。
    // 在这里我们先读到局部变量，最后再上锁写入 _data。
    CheckData stateDatas;
    
    for (int grp = 0; grp < 5; grp++) {
        int count = group_sizes_[grp];

        // 1. 填充 TX
        for(int k=0; k<count; k++) tx_buf[k] = cmd;

        // 2. 切换通道
        setMux(grp+3);

        // 3. SPI 传输
        memset(&t, 0, sizeof(t));
        t.length = count * 16;
        t.tx_buffer = tx_buf;
        t.rx_buffer = rx_buf;

        digitalWrite(PIN_ENC_CS, LOW);
        spi_device_polling_transmit(_spi, &t);
        digitalWrite(PIN_ENC_CS, HIGH);

        // 4. 解析数据 & 拓扑映射
        // AS5047P Daisy Chain: data[0] is from the LAST functionality device in chain
        for (int i = 0; i < count; i++) {
            uint16_t val = (rx_buf[i] << 8) | (rx_buf[i] >> 8); // 还原端序
            


            // 简单物理映射：假设数组按照物理连接顺序从前到后
            // rx_buf[0] 是链尾，rx_buf[count-1] 是链头
            int target_id = global_idx + (count - 1 - i);
            

            stateDatas.rawData[target_id] = val & 0x3FFF;
            // 解析诊断寄存器位


            stateDatas.magHigh[i] = (stateDatas.rawData[target_id] >> 11) & 0x01; // Bit 11
            stateDatas.magLow[i]  = (stateDatas.rawData[target_id] >> 10) & 0x01; // Bit 10
            stateDatas.cof[i]     = (stateDatas.rawData[target_id] >> 9) & 0x01; // Bit 9
            stateDatas.agc[i]     = stateDatas.rawData[target_id] & 0xFF;         // Bit 0-7

            // // 检查校验位 (Bit 15)
            // // 计算接收到的数据的校验 (包括数据位和错误标志位)
            // uint16_t data_no_parity = val & 0x7FFF;
            // uint8_t calc_parity = get_parity(data_no_parity);
            // uint8_t recv_parity = (val >> 15) & 0x01;
            // stateDatas.parityCheckFlags[target_id] = (calc_parity == recv_parity) ? 1 : 0;
        }
        global_idx += count;
    }

    // // 更新全局数据 (线程安全)
    // if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    //     memcpy(&_data, &tempDatas, sizeof(EncoderData));
    //     xSemaphoreGive(_mutex);
    // }
}

EncoderData HalEncoders::getData() {
    EncoderData ret;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        memcpy(&ret, &_data, sizeof(EncoderData));
        xSemaphoreGive(_mutex);
    } else {
        // 如果无法获取锁，返回空数据或旧数据
        memset(&ret, 0, sizeof(ret));
    }
    return ret;
}