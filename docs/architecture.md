# アーキテクチャ設計

## システム構成

```mermaid
graph TB
    subgraph Hardware
        IMU[IMUセンサー]
        TOF[ToFセンサー]
        MOTOR[モーター]
        LED[LED]
        BUZZER[ブザー]
    end

    subgraph Core
        MAIN[main.cpp]
        LOOP[main_loop.cpp]
        STAMP[stampfly.cpp]
    end

    subgraph Sensors
        SENSOR[sensor.cpp]
        IMU_C[imu.cpp]
        TOF_C[tof.cpp]
    end

    subgraph Control
        PID[pid.cpp]
        MOTOR_C[motor.cpp]
        RC[rc.cpp]
    end

    subgraph UI
        LED_C[led.cpp]
        BUZZER_C[buzzer.cpp]
        BUTTON[button.cpp]
    end

    subgraph Communication
        TELEM[telemetry.cpp]
    end

    %% 接続関係
    MAIN --> LOOP
    LOOP --> STAMP
    STAMP --> |制御| Control
    STAMP --> |センサー読取| Sensors
    Control --> |モーター制御| MOTOR
    Sensors --> |データ取得| IMU
    Sensors --> |データ取得| TOF
    TELEM --> |データ送信| Communication
    UI --> |状態表示| LED
    UI --> |音声通知| BUZZER
```

## ソースコードの構造

### コアモジュール
- `main.cpp`: エントリーポイント
- `main_loop.cpp`: 400Hz制御ループ
- `stampfly.cpp`: メインの制御ロジック

### センサー制御
- `sensor.cpp`: センサー統合管理
- `imu.cpp`: IMUセンサー制御
- `tof.cpp`: ToFセンサー制御

### 制御系
- `pid.cpp`: PID制御実装
- `motor.cpp`: モーター制御
- `rc.cpp`: リモートコントロール

### UI/フィードバック
- `led.cpp`: LED制御
- `buzzer.cpp`: ブザー制御
- `button.cpp`: ボタン制御

### 通信
- `telemetry.cpp`: テレメトリー通信

## データフロー

```mermaid
sequenceDiagram
    participant Main
    participant Loop
    participant Sensors
    participant Control
    participant Motors
    participant Telemetry

    Main->>Loop: 初期化
    loop 400Hz周期
        Loop->>Sensors: センサーデータ取得
        Sensors-->>Loop: IMU/ToFデータ
        Loop->>Control: 制御計算
        Control->>Motors: モーター出力設定
        Loop->>Telemetry: データ送信
    end
```

## 主要コンポーネントの責務

### メインループ (main_loop.cpp)
- 400Hz周期での制御実行
- センサーデータの取得
- 制御計算の実行
- モーター出力の設定
- テレメトリーデータの送信

### センサー管理 (sensor.cpp)
- 各種センサーの初期化
- データ取得のタイミング管理
- センサーデータの前処理
- 異常検知

### 制御系 (pid.cpp)
- PIDパラメータの管理
- 制御計算の実装
- 安定化制御
- フェールセーフ処理

### モーター制御 (motor.cpp)
- PWM出力の制御
- モーター回転数の管理
- 推力制御
- 安全機能

### テレメトリー (telemetry.cpp)
- データのパケット化
- 通信プロトコル管理
- エラー処理
- ログ記録
