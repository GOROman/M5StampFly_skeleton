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

#ifndef RC_HPP
#define RC_HPP

#include <stdio.h>
#include <stdint.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CONTROL_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TELEMETRY_CHAR_UUID "8b7c9c6a-c2dc-41e9-a087-7f4c2f9a75d0"

// Control channel indices
#define RUDDER         (0)
#define ELEVATOR       (1)
#define THROTTLE       (2)
#define AILERON        (3)
#define LOG            (4)
#define DPAD_UP        (5)
#define DPAD_DOWN      (6)
#define DPAD_LEFT      (7)
#define DPAD_RIGHT     (8)
#define BUTTON_ARM     (9)
#define BUTTON_FLIP    (10)
#define CONTROLMODE    (11)
#define ALTCONTROLMODE (12)

// Function declarations
void rc_init(void);
void rc_demo(void);
void rc_end(void);
uint8_t rc_isconnected(void);
void telemetry_send(uint8_t *data, uint16_t datalen);

// Global variables
extern volatile float Stick[16];
extern volatile uint8_t Rc_err_flag;
extern volatile bool BleConnected;

// BLE Server pointer
extern BLEServer* pServer;
extern BLECharacteristic* pControlCharacteristic;
extern BLECharacteristic* pTelemetryCharacteristic;

#endif