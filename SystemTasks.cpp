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

// --- 编码器任务 (300Hz) ---
void Task_Encoders(void* pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(3);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        encoders.readAll();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

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
void Task_CanBus(void* pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    RemoteCommand rxCmd;

    for (;;) {
        // 【关键修复】使用 static 避免栈溢出
        static EncoderData enc;
        static TactileData tac;

        // static int times;
        // 【新增】1. 维护总线状态 (自恢复、报警)
        bool isBusOk = twaiBus.maintain();

         if (isBusOk) {
        enc = encoders.getData();
        tac = tactile.getData();

        twaiBus.sendEncoderData(enc);
        twaiBus.sendTactileSummary(tac);

        // Serial.printf("第%d次can发送执行完毕\n",times);
        while (twaiBus.receiveMonitor(&rxCmd)) {
            // 处理指令
        }
                    
        } else {
        // >>> 总线故障中 <<<
        // 可以选择在这里闪烁 LED 指示故障
            vTaskDelay(pdMS_TO_TICKS(50)); // 故障时降低频率，给驱动恢复时间
        }
        // times++;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

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