#include <Arduino.h>
#include "Config.h"
#include "HalEncoders.h"
#include "HalTactile.h"
#include "HalTWAI.h"
#include "SystemTasks.h"
#include "Calibration.h"
#include "esp_task_wdt.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- System Boot (XSimple AI) ---");

      // 【修复】ESP-IDF v5.x 的看门狗初始化方式
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 5000,                    // 超时时间 5000ms (5秒)
        .idle_core_mask = (1 << 0) | (1 << 1), // 监控两个核心的 IDLE 任务
        .trigger_panic = true                  // 超时时触发 panic (打印堆栈)
    };
    esp_task_wdt_init(&wdt_config);


    // 【新增】启用系统日志，打印崩溃时的回溯信息
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("TWDT", ESP_LOG_ERROR);

    // 1. 硬件初始化
    encoders.begin();
    Serial.println("Encoders Init");
    
    // 触觉初始化 (注意: 确保 HalTactile 内部根据新的结构体分配了内存)
    tactile.begin(); 
    Serial.println("Tactile Init");
    
    // CAN 初始化
    if(twaiBus.begin()) {
        Serial.println("TWAI CAN Init OK");
    } else {
        Serial.println("TWAI CAN Init FAILED");
    }

    calibManager.begin();

    // 2. 启动 RTOS
    startSystemTasks();
    Serial.println("Tasks Started");
}

void handleSerialCommands() {
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c') {
            Serial.println("Calibrating...");
            
            // 安全挂起任务
            if(xEncTask) vTaskSuspend(xEncTask);
            if(xTacTask) vTaskSuspend(xTacTask);
            if(xCanTask) vTaskSuspend(xCanTask); // 【修复】现在能找到了

            EncoderData currentData = encoders.getData();
            calibManager.saveCurrentAsZero(currentData.rawAngles);
            
            if(xEncTask) vTaskResume(xEncTask);
            if(xTacTask) vTaskResume(xTacTask);
            if(xCanTask) vTaskResume(xCanTask);
            
            Serial.println("Done.");
        }
    }
}

void loop() {
    // 简单的串口命令处理，用于调试校准
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c') {
            Serial.println("Calibrating...");
            vTaskSuspend(xEncTask); // 挂起任务防止冲突
            vTaskSuspend(xCanTask);
            vTaskSuspend(xTacTask);

            
            calibManager.saveCurrentAsZero(encoders.getData().rawAngles);
            
            vTaskResume(xCanTask);
            vTaskResume(xEncTask);
            vTaskResume(xTacTask);
            Serial.println("Done.");
        }
    }
    vTaskDelay(100);

    static uint32_t lastPrintTime = 0;

    // 非阻塞式定时打印
    if (millis() - lastPrintTime > 500) {
        lastPrintTime = millis();
        printSystemMonitor();
    }


    handleSerialCommands();
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ==========================================
// [XSimple] 系统状态监视器
// ==========================================

void printSystemMonitor() {
    // 1. 获取最新数据快照 (线程安全)
    EncoderData enc = encoders.getData(); 
    // 注意：如果触觉未连接，请暂时注释掉下面这行，避免读取空指针
    // TactileData tac = tactile.getData(); 

    // 清屏 (部分串口助手支持 ANSI 码，不支持则忽略)
    // Serial.print("\033[2J\033[H"); 
    
    Serial.println("\n====================== [ XSimple Monitor ] ======================");

    // --- 区域 1: 编码器 (AS5047P) ---
    // 按每行 5 个打印，方便查看
    Serial.println(">>> Encoders (final Angle 0~16383)");
    for (int i = 0; i < ENCODER_TOTAL_NUM; i++) {
        // 格式: [ID:数值]
        // %02d 表示2位数字，%05d 表示5位数字对齐
        Serial.printf("[%02d:%05d] ", i, enc.finalAngles[i]); 
        
        // 每 5 个换行
        if ((i + 1) % 5 == 0) Serial.println();
    }
    Serial.println(); // 补一个换行

    // // --- 区域 2: 触觉传感器 (Global Forces) ---
    // // 为了防止串口堵塞，仅打印合力 (Fx, Fy, Fz)。
    // // 只有在调试具体点阵时才建议打印 detailed forces。
    // Serial.println(">>> Tactile Sensors (Global Force: x, y, z)");
    


    // //--- [新增区域] CAN 总线健康看板 ---
    // Serial.printf(">>> CAN Bus Status: %s\n", 
    //     status.state == TWAI_STATE_RUNNING ? "RUNNING" : 
    //     status.state == TWAI_STATE_BUS_OFF ? "BUS OFF (Error!)" : "STOPPED");
        
    // Serial.printf("    Tx Err: %d | Rx Err: %d | Rx Missed: %d\n", 
    //     status.tx_error_counter, status.rx_error_counter, status.rx_missed_count);
    
    // if(status.state == TWAI_STATE_BUS_OFF) {
    //     Serial.println("    [ALERT] SYSTEM IN AUTO-RECOVERY MODE");
    // }



    // 遍历所有组 (假设 Config.h 中定义了 TACTILE_GROUP_NUM)
    /* 
    // 【注意】如果触觉部分还有问题，请保持注释状态，先调通编码器
    for (int g = 0; g < TACTILE_GROUP_NUM; g++) {
        TacGroup *grp = &tac.groups[g];
        Serial.printf("--- Group %d ---\n", g);

        // 传感器 A (1610)
        Serial.printf("  [A] F:(%3d, %3d, %3d)\n", 
            grp->sensor_A.global[0], grp->sensor_A.global[1], grp->sensor_A.global[2]);

        // 传感器 B (1610)
        Serial.printf("  [B] F:(%3d, %3d, %3d)\n", 
            grp->sensor_B.global[0], grp->sensor_B.global[1], grp->sensor_B.global[2]);

        // 传感器 C (2015)
        Serial.printf("  [C] F:(%3d, %3d, %3d)\n", 
            grp->sensor_C.global[0], grp->sensor_C.global[1], grp->sensor_C.global[2]);
    }
    */
    
    Serial.println("=================================================================");
}
