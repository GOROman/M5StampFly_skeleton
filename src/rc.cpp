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

#include "rc.hpp"
#include "main_loop.hpp"

// Global variables
volatile float Stick[16] = {0};
volatile uint8_t Rc_err_flag = 0;
volatile bool BleConnected = false;

// BLE Server pointers
BLEServer* pServer = nullptr;
BLECharacteristic* pControlCharacteristic = nullptr;
BLECharacteristic* pTelemetryCharacteristic = nullptr;

// Connection counter for timeout detection
static uint32_t connectionCounter = 0;

/**
 * @brief BLEサーバーコールバッククラス
 */
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        BleConnected = true;
        connectionCounter = 0;
    };

    void onDisconnect(BLEServer* pServer) {
        BleConnected = false;
        pServer->startAdvertising();
    }
};

/**
 * @brief コントロールキャラクタリスティックコールバッククラス
 */
class ControlCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() == sizeof(float) * 16) {
            memcpy((void*)Stick, value.data(), sizeof(float) * 16);
            connectionCounter = 0;
        }
    }
};

/**
 * @brief ESP-NOW送信完了時のコールバック関数
 * @param mac_addr 送信先のMACアドレス
 * @param status 送信状態（成功/失敗）
 */
void on_esp_now_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

/**
 * @brief ESP-NOWデータ受信時のコールバック関数
 * 
 * 受信したデータを以下の順序で処理します：
 * 1. 送信元のMACアドレスを確認し、未登録の場合は登録
 * 2. 受信データの整合性チェック（MACアドレス、チェックサム）
 * 3. スティック入力とボタン状態の更新
 * 
 * @param mac_addr 送信元のMACアドレス
 * @param recv_data 受信データ
 * @param data_len データ長
 */
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *recv_data, int data_len) {
    Connect_flag = 0;

    uint8_t *d_int;
    // int16_t d_short;
    float d_float;

    if (!JoyAddr[0] && !JoyAddr[1] && !JoyAddr[2] && !JoyAddr[3] && !JoyAddr[4] && !JoyAddr[5]) {
        memcpy(JoyAddr, mac_addr, 6);
        memcpy((void*)peerInfo[JOY].peer_addr, JoyAddr, 6);
        peerInfo[JOY].channel = CHANNEL;
        peerInfo[JOY].encrypt = false;
        if (esp_now_add_peer(&peerInfo[JOY]) != ESP_OK) {
            USBSerial.println("Failed to add peer2");
            memset(JoyAddr, 0, 6);
        } else {
            esp_now_register_send_cb(on_esp_now_sent);
        }
    }

    Recv_MAC[0] = recv_data[0];
    Recv_MAC[1] = recv_data[1];
    Recv_MAC[2] = recv_data[2];

    if ((recv_data[0] == MyMacAddr[3]) && (recv_data[1] == MyMacAddr[4]) && (recv_data[2] == MyMacAddr[5])) {
        Rc_err_flag = 0;
    } else {
        Rc_err_flag = 1;
        return;
    }

    // checksum
    uint8_t check_sum = 0;
    for (uint8_t i = 0; i < 24; i++) check_sum = check_sum + recv_data[i];
    // if (check_sum!=recv_data[23])USBSerial.printf("checksum=%03d recv_sum=%03d\n\r", check_sum, recv_data[23]);
    if (check_sum != recv_data[24]) {
        Rc_err_flag = 1;
        return;
    }

    d_int         = (uint8_t *)&d_float;
    d_int[0]      = recv_data[3];
    d_int[1]      = recv_data[4];
    d_int[2]      = recv_data[5];
    d_int[3]      = recv_data[6];
    Stick[RUDDER] = d_float;

    d_int[0]        = recv_data[7];
    d_int[1]        = recv_data[8];
    d_int[2]        = recv_data[9];
    d_int[3]        = recv_data[10];
    Stick[THROTTLE] = d_float;

    d_int[0]       = recv_data[11];
    d_int[1]       = recv_data[12];
    d_int[2]       = recv_data[13];
    d_int[3]       = recv_data[14];
    Stick[AILERON] = d_float;

    d_int[0]        = recv_data[15];
    d_int[1]        = recv_data[16];
    d_int[2]        = recv_data[17];
    d_int[3]        = recv_data[18];
    Stick[ELEVATOR] = d_float;

    Stick[BUTTON_ARM]     = recv_data[19];//auto_up_down_status
    Stick[BUTTON_FLIP]    = recv_data[20];
    Stick[CONTROLMODE]    = recv_data[21];//Mode:rate or angle control
    Stick[ALTCONTROLMODE] = recv_data[22];//高度制御

    uint8_t ahrs_reset_flag = recv_data[23];

    Stick[LOG] = 0.0;
    // if (check_sum!=recv_data[23])USBSerial.printf("checksum=%03d recv_sum=%03d\n\r", check_sum, recv_data[23]);

#if 0
  USBSerial.printf("%6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f  %6.3f\n\r", 
                                            Stick[THROTTLE],
                                            Stick[AILERON],
                                            Stick[ELEVATOR],
                                            Stick[RUDDER],
                                            Stick[BUTTON_ARM],
                                            Stick[BUTTON_FLIP],
                                            Stick[CONTROLMODE],
                                            Stick[ALTCONTROLMODE],
                                            Stick[LOG]);
#endif
}

// 送信コールバック
void on_esp_now_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    esp_now_send_status = status;
    memcpy((void*)SendAddress, mac_addr, 6);
    #if 0
    if (status == ESP_NOW_SEND_SUCCESS)
        USBSerial.printf("MAC ADDRES %02X:%02X:%02X:%02X:%02X:%02X ESP_NOW_SEND_SUCCESS!\n\r", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    else
        USBSerial.printf("MAC ADDRES %02X:%02X:%02X:%02X:%02X:%02X ESP_NOW_SEND_FAIL!\n\r", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    #endif
}

/**
 * @brief リモートコントロールシステムの初期化
 * 
 * 以下の初期化処理を実行します：
 * 1. スティック入力の初期化
 * 2. ESP-NOWの初期化とWiFiのSTAモード設定
 * 3. MACアドレスの取得と表示
 * 4. ブロードキャストピアの設定
 * 5. 自身のMACアドレスの通知
 */
void rc_init(void) {
    // スティック入力の初期化
    for (uint8_t i = 0; i < 16; i++) Stick[i] = 0.0;

    // ESP-NOWの初期化とWiFiのSTAモード設定
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    WiFi.macAddress((uint8_t *)MyMacAddr);
    USBSerial.printf("MAC ADDRESS: %02X:%02X:%02X:%02X:%02X:%02X\r\n", MyMacAddr[0], MyMacAddr[1], MyMacAddr[2],
                     MyMacAddr[3], MyMacAddr[4], MyMacAddr[5]);

    if (esp_now_init() == ESP_OK) {
        USBSerial.println("ESPNow Init Success");
    } else {
        USBSerial.println("ESPNow Init Failed");
        ESP.restart();
    }

    // MACアドレスブロードキャスト
    uint8_t addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy((void*)peerInfo[JOY].peer_addr, addr, 6);
    peerInfo[JOY].channel = CHANNEL;
    peerInfo[JOY].encrypt = false;
    if (esp_now_add_peer(&peerInfo[JOY]) != ESP_OK) {
        USBSerial.println("Failed to add peer");
        return;
    }
    esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Send my MAC address
    for (uint16_t i = 0; i < 50; i++) {
        send_peer_info();
        delay(50);
        //USBSerial.printf("%d\n", i);
    }

    // ESP-NOW再初期化
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) {
        USBSerial.println("ESPNow Init Success2");
    } else {
        USBSerial.println("ESPNow Init Failed2");
        ESP.restart();
    }

    //テレメトリーM5ATOMとペアリング
    memcpy((void*)peerInfo[TELEM].peer_addr, TelemAddr, 6);
    peerInfo[TELEM].channel = CHANNEL;
    peerInfo[TELEM].encrypt = false;
    if (esp_now_add_peer(&peerInfo[TELEM]) != ESP_OK) 
    {
            USBSerial.println("Failed to telemetry add peer2");
    }
    else USBSerial.printf("Telemetry peering Sucess!\n\r");
    esp_now_register_send_cb(on_esp_now_sent);

    // ESP-NOWコールバック登録
    esp_now_register_recv_cb(OnDataRecv);
    USBSerial.println("ESP-NOW Ready.");
}

/**
 * @brief 自身の情報をピアに送信
 * 
 * 以下の情報を送信します：
 * - 使用するWiFiチャネル
 * - 自身のMACアドレス
 * - ピアコマンド情報
 */
void send_peer_info(void) {
    uint8_t data[11];
    data[0] = CHANNEL;                              // WiFiチャネル
    memcpy(&data[1], (uint8_t *)MyMacAddr, 6);     // MACアドレス
    memcpy(&data[1 + 6], (uint8_t *)peer_command, 4);  // ピアコマンド
    esp_now_send(peerInfo[JOY].peer_addr, data, 11);
}

/**
 * @brief テレメトリーデータの送信とエラー処理
 * 
 * 以下の機能を提供します：
 * - ESP-NOWを使用したデータ送信
 * - 送信エラーの検出と処理
 * - 自動的なエラー状態からの復帰（500カウント後）
 * 
 * @param peerInfo 送信先のESP-NOWピア情報
 * @param data 送信データ
 * @param datalen データ長
 * @return uint8_t エラー状態（0: 正常, 1: エラー）
 */
uint8_t telemetry_send(esp_now_peer_info_t* peerInfo, uint8_t *data, uint16_t datalen) {
    static uint32_t cnt       = 0;  // カウンタ
    static uint8_t error_flag = 0;  // エラー状態フラグ
    static uint8_t state      = 0;  // 送信状態

    esp_err_t result;

    // エラーがなく、送信可能な状態の場合に送信を実行
    if ((error_flag == 0) && (state == 0)) {
        result = esp_now_send(peerInfo->peer_addr, data, datalen);
        cnt    = 0;
        //USBSerial.printf("%d\n\r", result);
    } else
        cnt++;

    // 送信結果に基づいてエラー状態を更新
    if (esp_now_send_status == ESP_NOW_SEND_SUCCESS) {
        error_flag = 0;  // 送信成功
        // state = 0;
    } else {
        error_flag = 1;  // 送信失敗
        // state = 1;
    }

    // 500カウント経過後にエラー状態をリセット
    if (cnt > 500) {
        error_flag = 0;
        cnt        = 0;
    }
    cnt++;
    //USBSerial.printf("%6d %d %d\r\n", cnt, error_flag, esp_now_send_status);

    return error_flag;
}

void rc_end(void) {
    // Ps3.end();
}

/**
 * @brief リモートコントロールの接続状態を確認
 * 
 * Connect_flagを使用して接続状態を判定します。
 * 40カウント以上経過した場合、切断と判断します。
 * 
 * @return uint8_t 接続状態（1: 接続中, 0: 切断）
 */
uint8_t rc_isconnected(void) {
    bool status;
    Connect_flag++;
    if (Connect_flag < 40)  // 40カウント未満は接続中と判断
        status = 1;
    else
        status = 0;  // 40カウント以上は切断と判断
    // USBSerial.printf("%d \n\r", Connect_flag);
    return status;
}

void rc_demo() {
}
