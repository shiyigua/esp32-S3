// #include "SystemTasks.h"
// #include "Config.h"
// #include "HalEncoders.h"
// #include "HalTactile.h"
// #include "Calibration.h" // 如果需要在任务中直接读取校准值


// // 任务句柄
// TaskHandle_t xEncTask = NULL;
// TaskHandle_t xTacTask = NULL;



// // --- 编码器任务 (高频 300Hz) ---
// void Task_Encoders(void *pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(1000 / 300); // 300Hz -> ~3.33ms
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         // 调用底层的读所有函数
//         encoders.readAll();
        
//         // 绝对延时
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// // --- 触觉任务 (低频 50Hz) ---
// void Task_Tactile(void *pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(200); // 200ms(暂定，方便任务调度安排)
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         //暂时屏蔽触觉任务，方便系统调度
//         //tactile.update();
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// void startSystemTasks() {
//     xTaskCreatePinnedToCore(Task_Encoders, "EncTask", 4096, NULL, TASK_ENC_PRIO, &xEncTask, TASK_ENC_CORE);
//     xTaskCreatePinnedToCore(Task_Tactile, "TacTask", 4096, NULL, TASK_TAC_PRIO, &xTacTask, TASK_TAC_CORE);
// }


// #include "SystemTasks.h"
// #include "HalEncoders.h"
// #include "HalTactile.h"
// #include "HalTWAI.h"

// TaskHandle_t xEncTask = NULL;
// TaskHandle_t xTacTask = NULL;
// TaskHandle_t xCanTask = NULL;

// // 编码器读取 (300Hz ~ 500Hz)
// void Task_Encoders(void *pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(3); // 3ms
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         encoders.readAll(); // SPI 交互
        
//         // 数据读完后，直接发出信号或者由 CAN 任务定时去取
//         // 为了降低延迟，可以在这里直接调 CAN 发送 (如果是高优先级核心)
//         // 但为了安全，我们用队列或让 CAN 任务自取
        
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// // 触觉读取 (50Hz)
// void Task_Tactile(void *pvParameters) {
//     const TickType_t xFrequency = pdMS_TO_TICKS(20); 
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         tactile.update(); 
//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }

// // CAN 通信任务 (100Hz)
// void Task_CanBus(void *pvParameters) {
//     TickType_t xLastWakeTime = xTaskGetTickCount();
//     const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms

//     RemoteCommand rxCmd; // 定义接收缓存
//     for(;;) {
//         // 1. 获取数据快照 (线程安全拷贝)
//         EncoderData enc = encoders.getData();
//         TactileData tac = tactile.getData(); // 假设 HalTactile 有互斥锁保护

//         // 2. 发送关键控制数据 (高优先级)
//         twaiBus.sendEncoderData(enc);     // 角度
//         twaiBus.sendTactileSummary(tac);  // 合力

//         // 3. 详细触觉数据 (可选，按需降频发送)
//         // 由于 1500 字节太大，建议每 10 次循环发送一次，或者只发变化量
//         // static int div = 0;
//         // if(div++ > 10) { twaiBus.sendTactileFullDump(tac); div=0; }

//         // 4. 接收数据 (传入地址)
//         while(twaiBus.receiveMonitor(&rxCmd)) {
//         // 处理接收到的指令...

//         vTaskDelayUntil(&xLastWakeTime, xFrequency);
//     }
// }
// }

// void startSystemTasks() {
//     xTaskCreatePinnedToCore(Task_Encoders, "EncTask", 4096, NULL, 10, &xEncTask, 0);
//     xTaskCreatePinnedToCore(Task_Tactile,  "TacTask", 8192, NULL, 5,  &xTacTask, 1); // 栈加大点
//     xTaskCreatePinnedToCore(Task_CanBus,   "CanTask", 4096, NULL, 5,  &xCanTask, 0); 
// }

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