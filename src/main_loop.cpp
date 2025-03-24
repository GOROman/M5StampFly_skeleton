/*
 * MIT License
 *
 * Copyright (c) 2024 Kouhei Ito
 * Copyright (c) 2024 M5Stack
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

//
// StampFly Flight Control Main Module
//
// Desigend by Kouhei Ito 2023~2024
//
// 2024-08-11 StampFly 自己開発用のスケルトンプログラム制作開始

#include "main_loop.hpp"
#include "motor.hpp"
#include "rc.hpp"
#include "pid.hpp"
#include "sensor.hpp"
#include "led.hpp"
#include "telemetry.hpp"
#include "button.hpp"
#include "buzzer.h"
#include "stampfly.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


volatile uint8_t armButtonState = 0;
volatile uint8_t armButtonPressedAndRerleased = 0;
volatile uint8_t previousArmButtonState = 0; 

void IRAM_ATTR onTimer(void);
void init_copter(void);
void update_loop400Hz(void);
void init_mode(void);
void average_mode(void);
void flight_mode(void);
void parking_mode(void);
void loop_400Hz(void);
float limit(float value, float min, float max);

/**
 * @brief メインの制御ループ関数（400Hz周期で実行）
 * 
 * この関数は以下の処理を実行します：
 * 1. センサー値の更新とIMUの姿勢推定
 * 2. 現在のモードに応じた制御処理の実行
 * 3. テレメトリーデータの送信
 */
void loop_400Hz(void) {
    // センサー値の更新とIMUの姿勢推定を実行
    update_loop400Hz();
    
    // 現在のモードに応じて適切な制御処理を実行
    if (StampFly.flag.mode == INIT_MODE) 
        init_mode();          // 初期化モード
    else if (StampFly.flag.mode == AVERAGE_MODE)
        average_mode();       // センサーオフセット計算モード
    else if (StampFly.flag.mode == FLIGHT_MODE)
        flight_mode();        // 飛行モード
    else if (StampFly.flag.mode == PARKING_MODE)
        parking_mode();       // 着陸/停止モード

    // テレメトリーデータの送信
    telemetry();
    StampFly.flag.oldmode = StampFly.flag.mode;  // 現在のモードを記録
}

// 割り込み関数
// Intrupt function
hw_timer_t* timer = NULL;
void IRAM_ATTR onTimer(void) {
    StampFly.flag.loop = 1;
    //loop_400Hz();
}

/**
 * @brief StampFlyの初期化処理
 * 
 * 以下の初期化を行います：
 * - 動作モードの初期化
 * - LEDの初期化
 * - シリアル通信の初期化
 * - 各種センサーの初期化
 * - モーター制御の初期化
 */
void init_copter(void) {
    //disableCore1WDT();
    // 動作モードを初期化モードに設定
    StampFly.flag.mode = INIT_MODE;
    StampFly.flag.loop = 0;
    // LEDの初期化
    led_init();
    // シリアル通信の初期化（115200bps）
    USBSerial.begin(115200);
    delay(1500);
    USBSerial.printf("Start StampFly! Skeleton\r\n");
    motor_init();
    sensor_init();
    rc_init();

    // init button G0
    init_button();
    setup_pwm_buzzer();
    USBSerial.printf("Finish StampFly init!\r\n");
    start_tone();

    // 割り込み設定
    // Initialize intrupt
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 2500, true);
    timerAlarmEnable(timer);

}

//loop400Hzの更新関数
void update_loop400Hz(void) {
    uint32_t now_time;

    while (StampFly.flag.loop == 0);
    StampFly.flag.loop = 0;

    #if 0
    USBSerial.printf("%9.4f %9.4f %04d\n\r", 
        StampFly.times.elapsed_time, 
        StampFly.times.interval_time,
        StampFly.sensor.bottom_tof_range);
    #endif

    //Clock
    now_time = micros();
    StampFly.times.old_elapsed_time = StampFly.times.elapsed_time;
    StampFly.times.elapsed_time = 1e-6 * (now_time - StampFly.times.start_time);
    StampFly.times.interval_time = StampFly.times.elapsed_time - StampFly.times.old_elapsed_time;
    
    // Read Sensor Value
    sensor_read(&StampFly.sensor);
    
    // LED Drive
    led_drive();

    // Read Button Value
    armButtonState = Stick[BUTTON_ARM];
    if (armButtonState != previousArmButtonState) {
        if (armButtonState == 0) {
            armButtonPressedAndRerleased = 1;
        }
        previousArmButtonState = armButtonState;
    }
}

void init_mode(void) {
    motor_stop();
    StampFly.counter.offset = 0;
    //Mode change
    StampFly.flag.mode = AVERAGE_MODE;
    return;

}

void average_mode(void) {
    // Gyro offset Estimate 角速度のオフセットを取得
    // Set LED Color
    onboard_led1(PERPLE, 1);
    onboard_led2(PERPLE, 1);

    if (StampFly.counter.offset < AVERAGENUM) {
        sensor_calc_offset_avarage();
        StampFly.counter.offset++;
        return;
    }
    // Mode change
    StampFly.flag.mode   = PARKING_MODE;
    StampFly.times.start_time = micros();
    StampFly.times.old_elapsed_time = 0.0f;
    return;
}


void flight_mode(void) {
    //飛行するためのコードを以下に記述する
    // Set LED Color
    onboard_led1(YELLOW, 1);
    onboard_led2(YELLOW, 1);
    float throttle_delta = limit(Stick[THROTTLE], 0.0, 0.9);

    motor_set_duty_fl(throttle_delta);
    motor_set_duty_fr(throttle_delta);
    motor_set_duty_rl(throttle_delta);
    motor_set_duty_rr(throttle_delta);
    if (armButtonPressedAndRerleased)StampFly.flag.mode = PARKING_MODE;
    armButtonPressedAndRerleased = 0;
}

void parking_mode(void) {
    //着陸している時に行う処理を記述する
    // Set LED Color
    onboard_led1(GREEN, 1);
    onboard_led2(GREEN, 1);
    
    motor_stop();
    if (armButtonPressedAndRerleased)StampFly.flag.mode = FLIGHT_MODE;
    armButtonPressedAndRerleased = 0;
}

float limit(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
