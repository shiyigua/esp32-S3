#include "SystemTasks.h"
#include "Config.h"
#include "HalEncoders.h"
#include "HalTactile.h"
#include "HalTWAI.h"
#include "Calibration.h"

// 任务句柄 (全局)
TaskHandle_t xEncTask = NULL;
TaskHandle_t xTacTask = NULL;
TaskHandle_t xCanTask = NULL;  // 【修复】添加 CAN 任务句柄

// 全局实例
// HalEncoders& encoders = HalEncoders::getInstance();
CalibrationManager calibrationManager;

// 队列
QueueHandle_t xQueueEncoderData = NULL;

// 控制标志
volatile bool g_requestZeroCalibration = false;

// --- 编码器任务 (200Hz) ---
void Task_Encoders(void *pvParameters) {
    (void)pvParameters;
    
    encoders.begin();
    calibrationManager.begin();
    
    EncoderData localData;
    uint16_t calibrated[ENCODER_TOTAL_NUM];
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5);

    for (;;) {
        // 1. 获取数据 (包含自动错误检测与恢复)
        encoders.getData(localData);

        // 2. 处理归零请求
        if (g_requestZeroCalibration) {
            calibrationManager.saveCurrentAsZero(localData.rawAngles);
            g_requestZeroCalibration = false;
        }

        // 3. 校准
        calibrationManager.calibrateAll(localData.rawAngles, calibrated);

        // 4. 填充finalAngles
        for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
            if (localData.errorFlags[i]) {
                localData.finalAngles[i] = 0xFFFF;  // 错误标记
            } else {
                localData.finalAngles[i] = calibrated[i];
            }
        }

        // 5. 发送到队列
        if (xQueueEncoderData) {
            xQueueOverwrite(xQueueEncoderData, &localData);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
// void Task_Encoders(void* pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(3);
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         encoders.readAll();
//         calibManager.calibrateAll(encoders.getData().rawAngles, encoders.getData().finalAngles);
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// --- 触觉任务 (50Hz) ---
void Task_Tactile(void* pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(20);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // tactile.update();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- CAN 通信任务 (100Hz) ---
void Task_CanBus(void *pvParameters) {
    (void)pvParameters;
    
    twaiBus.begin();
    
    EncoderData txData;
    RemoteCommand cmd{};
    static uint8_t errorSendCounter = 0;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);

    for (;;) {
        // 1. 从队列获取最新数据
        if (xQueueEncoderData && xQueueReceive(xQueueEncoderData, &txData, 0) == pdTRUE) {
            
            // 2. 发送角度数据 (使用原有打包方式)
            twaiBus.sendEncoderData(txData);
            
            // 3. 【新增】每隔5帧检查并发送错误状态
            errorSendCounter++;
            if (errorSendCounter >= 5) {
                errorSendCounter = 0;
                
                // 检查是否有错误
                bool hasError = false;
                for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
                    if (txData.errorFlags[i]) {
                        hasError = true;
                        break;
                    }
                }
                
                if (hasError) {
                    twaiBus.sendErrorStatus(txData);
                }
            }
        }

        // 4. 接收处理
        twai_message_t rxMsg;
        while (twaiBus.receiveMonitor(&cmd)) {
            // 处理远程命令
            if (rxMsg.identifier == 0x200 && rxMsg.data_length_code >= 1) {
                if (rxMsg.data[0] == 0x01) {
                    g_requestZeroCalibration = true;
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
// // --- CAN 通信任务 (100Hz) ---
// void Task_CanBus(void* pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(10);
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     RemoteCommand rxCmd;

//     for (;;) {
//         // 【关键修复】使用 static 避免栈溢出
//         static EncoderData enc;
//         static TactileData tac;

//         // static int times;
//         // 【新增】1. 维护总线状态 (自恢复、报警)
//         bool isBusOk = twaiBus.maintain();

//          if (isBusOk) {
//         enc = encoders.getData();
//         tac = tactile.getData();

//         twaiBus.sendEncoderData(enc);
//         // twaiBus.sendTactileSummary(tac);

//         // Serial.printf("第%d次can发送执行完毕\n",times);
//         while (twaiBus.receiveMonitor(&rxCmd)) {
//             // 处理指令
//         }
                    
//         } else {
//         // >>> 总线故障中 <<<
//         // 可以选择在这里闪烁 LED 指示故障
//             vTaskDelay(pdMS_TO_TICKS(50)); // 故障时降低频率，给驱动恢复时间
//         }
//         // times++;
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// 启动任务
void startSystemTasks() {
    // 编码器任务 - Core 1, 高优先级
    xTaskCreatePinnedToCore(
        Task_Encoders, "EncTask", 4096, NULL, 10, &xEncTask, 1
    );

    // 触觉任务 - Core 0, 中优先级, 大栈
    xTaskCreatePinnedToCore(
        Task_Tactile, "TacTask", 8192, NULL, 5, &xTacTask, 0
    );

    // CAN 任务 - Core 0, 中优先级
    xTaskCreatePinnedToCore(
        Task_CanBus, "CanTask", 4096, NULL, 5, &xCanTask, 0
    );
}