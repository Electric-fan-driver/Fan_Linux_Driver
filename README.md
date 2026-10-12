# 🍃 Smart Fan Project (스마트 선풍기)

NVIDIA Jetson 기반 임베디드 리눅스 환경에서 커스텀 디바이스 드라이버와 FSM(유한 상태 머신) 기반 제어 애플리케이션을 통해 구현한 스마트 선풍기 시스템입니다.

---

## 📌 프로젝트 개요 (Overview)

- **타깃 플랫폼**: NVIDIA Jetson Orin 시리즈 (JetPack 6.2.x, L4T R36.5.2, Kernel 5.15.199-tegra)
- **주요 개발 내용**:
  - 디바이스 트리 오버레이(DTBO) 작성 및 하드웨어 핀 배정
  - 커스텀 캐릭터 디바이스 드라이버(LKM) 및 커널 타이머/워크큐(`delayed_work`) 구현
  - `pthread` 및 FSM 기반 유저 공간 통합 제어 애플리케이션 개발
  - 드라이버 일괄 적재/해제를 위한 쉘 스크립트 구성

---

## 🛠 시스템 아키텍처 및 하드웨어 구성

### 1. 하드웨어 핀 배정 (Pinout)

| 기능 | 헤더 핀 | 칩 내부 이름 |
| :--- | :--- | :--- |
| **I2C SDA / SCL (BMP180)** | 3 / 5 | (i2c-7) |
| **I2C SDA / SCL (LCD)** | 27 / 28 | (i2c-1) |
| **모터 PWM (L298N ENA)** | 32 | PG.06 (PWM7) |
| **서보 PWM** | 33 | PH.00 (PWM5) |
| **L298N IN1 / IN2** | 37 / 40 | PY.02 / PI.00 |
| **전원 버튼** | 29 | PQ.05 |
| **회전 버튼** | 31 | PQ.06 |
| **풍량 엔코더 A / B / SW** | 11 / 36 / 7 | PR.04 / PR.05 / PAC.06 |
| **타이머 엔코더 A / B / SW** | 12 / 35 / 38 | PH.07 / PI.02 / PI.01 |
| **LED 1 / 2 / 3 / 4단계** | 13 / 16 / 18 / 22 | PY.00 / PY.04 / PY.03 / PY.01 |
| **전원** | 5 V: 미정 / 3.3 V: 미정 / GND: 미정 | - |
| **예비** | 15, 19, 21, 23, 24, 26 | - |

### 2. 커널 드라이버 및 인터페이스 명세

BMP180을 포함한 장치들은 Character Device 인터페이스를 통해 사용자 공간과 데이터를 주고받습니다.

| 장치 노드 | 소스 파일 | 통신 방식 | 설명 |
| :--- | :--- | :--- | :--- |
| `/dev/fan_input` | `fan_input/` 관련 소스 | `read`, `poll` | 버튼·엔코더 이벤트 전달 및 소프트웨어 디바운스 처리 |
| `/dev/fan_motor` | `motor/fan_motor_main.c` | `ioctl` | 모터 풍량 레벨(0~4) 제어 |
| `/dev/fan_led` | `led/fan_led_main.c` | `ioctl` | 풍량 단계에 따른 LED 점등 제어 |
| `/dev/bmp180` | `bmp180/fan_bmp180_main.c`, `fan_bmp180_sensor.c`, `fan_bmp180_device.c` | `read` | 보정된 온도를 0.1°C 단위의 정수 문자열로 반환 |
| `/dev/lcd1602` | `lcd1602/fan_lcd_main.c`, `fan_lcd_hw.c`, `fan_lcd_device.c` | `pwrite`, `ioctl` | PCF8574T 기반 HD44780 제어, 영역별 부분 갱신 및 백라이트 제어 |

---

## ✨ 핵심 기능 (Key Features)

### 1. FSM 기반 통합 제어

- **Power FSM**: 전원 ON/OFF 시 풍량 및 회전 상태 초기화.
- **Mode FSM**:
  - **수동(MAN) 모드**: 풍량 엔코더로 풍량 단계 직접 조작.
  - **자동(AUTO) 모드**: BMP180 온도를 1초마다 읽어 온도 구간에 따른 풍량 자동 설정.
- **Timer FSM**:
  - 타이머 엔코더로 시간 설정 (최대 12시간).
  - 설정 시간에 따라 카운트다운 및 종료 처리.
- **Swing FSM**: 회전 버튼을 통한 회전 ON/OFF 제어.

### 2. LCD 16x2 부분 갱신 (Flicker-Free)

화면 전체를 지우지 않고 변경된 구역만 `pwrite()`의 offset을 통해 갱신합니다.

```text
col: 0123456789012345
1행: AUTO    00:30:00   <-- [0~3: 모드] / [8~15: 타이머]
2행: 24.6°C       ROT   <-- [16~22: 온도] / [29~31: 회전 표시]
```

---

## 📂 프로젝트 구조 (Project Structure)

```text
Fan_Linux_Driver/
├── app/
│   ├── include/
│   │   ├── fan_api.h           # 애플리케이션 모듈 인터페이스
│   │   ├── shared_state.h      # 시스템 공유 상태
│   │   └── thread.h            # 스레드 관련 선언
│   ├── src/
│   │   ├── bmp180.c            # 온도 센서 모듈 구현
│   │   ├── bmp180_policy.c     # 온도 기반 자동 풍량 정책
│   │   ├── controller.c        # 하드웨어 제어
│   │   ├── display.c           # LCD 출력 모듈 구현
│   │   ├── fsm.c               # FSM 이벤트 및 상태 처리
│   │   ├── input.c             # 버튼/엔코더 입력 처리
│   │   ├── led.c               # LED 제어
│   │   ├── main.c              # 통합 애플리케이션 진입점
│   │   ├── motor.c             # 모터 구동 인터페이스
│   │   ├── thread.c            # 입력/타이머/센서 스레드
│   │   └── timer.c             # 타이머 로직
│   ├── tests/
│   │   ├── bmp180_test.c       # BMP180 온도 읽기 테스트
│   │   ├── lcd_test.c          # 사용자 공간 직접 I2C 출력 테스트
│   │   ├── lcd_device_test.c   # LCD 문자 장치 pwrite/ioctl 테스트
│   │   └── lcd_display_test.c  # 앱 API로 가상 온도 표시 테스트
│   └── Makefile
├── driver/
│   ├── Makefile                # 커널 모듈 빌드
│   ├── bmp180/
│   │   ├── fan_bmp180.h
│   │   ├── fan_bmp180_main.c   # DT 매칭, probe/remove
│   │   ├── fan_bmp180_sensor.c # 센서 측정 및 보정
│   │   └── fan_bmp180_device.c # /dev/bmp180 read 인터페이스
│   ├── lcd1602/
│   │   ├── fan_lcd.h
│   │   ├── fan_lcd_main.c      # DT 매칭, probe/remove
│   │   ├── fan_lcd_hw.c        # PCF8574T 및 HD44780 제어
│   │   └── fan_lcd_device.c    # /dev/lcd1602 pwrite/ioctl
│   ├── fan_input/              # 입력 이벤트 처리 관련 드라이버
│   ├── fan_input_driver.c
│   ├── led/
│   │   └── fan_led_main.c
│   ├── motor/
│   │   └── fan_motor_main.c
│   ├── servo/
│   │   └── fan_servo_main.c
│   └── init.txt
├── dts/
│   ├── extracted.dts
│   ├── fan-overlay.dts         # 디바이스 트리 오버레이
│   └── fan-overlay.preprocessed.dts
├── include/
│   └── fan_ioctl.h            # 공용 ioctl 및 이벤트 정의
├── scripts/
│   ├── load_all.sh             # 드라이버 적재 스크립트
│   └── unload_all.sh           # 드라이버 해제 스크립트
├── Makefile
├── .gitignore
└── README.md
```

---

## 🚀 빌드 및 실행 방법 (Quick Start)

### 1. 디바이스 트리 오버레이 적용

```bash
# 원본 DTS 전처리 후 DTBO 컴파일
# (또는 최신 전처리 결과 파일을 사용할 경우)
dtc -@ -I dts -O dtb -o fan-overlay.dtbo dts/fan-overlay.preprocessed.dts

# DTBO 복사 후 부팅 설정에 오버레이 등록 및 재부팅
sudo cp fan-overlay.dtbo /boot/
```

### 2. 드라이버 빌드 및 모듈 적재

```bash
# 커널 모듈 빌드
make -C driver

# 모듈 적재/해제 스크립트 (현재 등록 방식에 맞게 수정 필요)
sudo ./scripts/load_all.sh
sudo ./scripts/unload_all.sh
```

### 3. 애플리케이션 빌드 및 실행

```bash
cd app
make
sudo ./main
```

※ 현재 모듈 적재 스크립트의 수동 장치 노드 생성 부분은 `miscdevice` 기반 드라이버와 일치하지 않아 수정이 필요합니다.
