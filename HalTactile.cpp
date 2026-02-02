// #include "HalTactile.h"
// #include <cstring>

// HalTactile tactile;

// HalTactile::HalTactile() {
//     _mutex = xSemaphoreCreateMutex();
// }

// void HalTactile::begin() {
//     pinMode(PIN_TAC_CS_A, OUTPUT);
//     digitalWrite(PIN_TAC_CS_A, HIGH);

//     spi_bus_config_t buscfg = {};
//     buscfg.mosi_io_num = PIN_TAC_MOSI;
//     buscfg.miso_io_num = PIN_TAC_MISO;
//     buscfg.sclk_io_num = PIN_TAC_SCLK;
//     buscfg.quadwp_io_num = -1;
//     buscfg.quadhd_io_num = -1;
//     buscfg.max_transfer_sz = 32;

//     // 使用 SPI3 (VSPI)
//     spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

//     spi_device_interface_config_t devcfg = {};
//     devcfg.clock_speed_hz = SPI_FREQ_TAC;
//     devcfg.mode = 0; // 假设 Mode 0
//     devcfg.spics_io_num = -1;
//     devcfg.queue_size = 1;

//     spi_bus_add_device(SPI3_HOST, &devcfg, &_spi);
// }

// void HalTactile::update() {
//     // 模拟读取
//     uint8_t tx[10] = {0xAA};
//     uint8_t rx[10] = {0};
    
//     spi_transaction_t t = {};
//     t.length = 8 * 10;
//     t.tx_buffer = tx;
//     t.rx_buffer = rx;

//     digitalWrite(PIN_TAC_CS_A, LOW);
//     spi_device_polling_transmit(_spi, &t);
//     digitalWrite(PIN_TAC_CS_A, HIGH);

//     if (xSemaphoreTake(_mutex, 10)) {
//     _data.groups[0].sensor_A.global[0] = 0; 
//         xSemaphoreGive(_mutex);
//     }
// }

// TactileData HalTactile::getData() {
//     TactileData ret;
//     if (xSemaphoreTake(_mutex, 10)) {
//         ret = _data;
//         xSemaphoreGive(_mutex);
//     }
//     return ret;
// }
#include "HalTactile.h"
#include <cstring>

// 全局实例
HalTactile tactile;

HalTactile::HalTactile() {
    _mutex = xSemaphoreCreateMutex();
    // 在堆上分配大结构体，避免栈溢出
    _data = new TactileData();
    memset(_data, 0, sizeof(TactileData));
}

void HalTactile::begin() {
    pinMode(PIN_TAC_CS_A, OUTPUT);
    digitalWrite(PIN_TAC_CS_A, HIGH);
    pinMode(PIN_TAC_CS_B, OUTPUT);
    digitalWrite(PIN_TAC_CS_B, HIGH);

    // SPI 初始化代码放这里...
}

void HalTactile::update() {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10))) {
        // 模拟数据更新 (实际替换为 SPI 读取)
        _data->groups[0].sensor_A.global[0]++;

        xSemaphoreGive(_mutex);
    }
}

TactileData HalTactile::getData() {
    // 使用 static 避免在栈上分配大结构体
    static TactileData ret;
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10))) {
        ret = *_data;
        xSemaphoreGive(_mutex);
    }
    return ret;
}