# 🍃 Smart Fan Project (스마트 선풍기)

NVIDIA Jetson 기반 임베디드 리눅스 환경에서 커스텀 디바이스 드라이버와 FSM(유한 상태 머신) 기반 제어 애플리케이션을 통해 구현한 스마트 선풍기 시스템입니다.

---

## 📌 프로젝트 개요 (Overview)

* **타깃 플랫폼**: NVIDIA Jetson Orin 시리즈 (JetPack 6.2.x, L4T R36.5.2, Kernel 5.15.199-tegra)
* **주요 개발 내용**:
  * 디바이스 트리 오버레이(DTBO) 작성 및 하드웨어 핀 배정
  * 커스텀 캐릭터 디바이스 드라이버(LKM) 및 커널 타이머/워크큐(`delayed_work`) 구현
  * `epoll` 및 FSM 기반 유저 공간 통합 제어 애플리케이션 개발
  * 드라이버 일괄 적재/해제를 위한 자동화 쉘 스크립트 제공

---

## 🛠 시스템 아키텍처 및 하드웨어 구성

### 1. 하드웨어 핀 배정 (Pinout)

| 기능 | 헤더 핀 | 칩 내부 이름 |
| :--- | :--- | :--- |
| **I2C SDA / SCL (BMP180, LCD)** | 3 / 5 | (i2c-7) |
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

모든 드라이버는 `miscdevice`로 동적 등록되며 공용 헤더(`fan_ioctl.h`)를 통해 통신합니다.

| 장치 노드 | 소스 파일 | 통신 방식 | 설명 |
| :--- | :--- | :--- | :--- |
| `/dev/fan_input` | `fan_input_driver.c` | `read`, `poll` | 8바이트 이벤트 구조체(`struct fan_input_event`) 반환, 소프트웨어 디바운스 적용 |
| `/dev/fan_motor` | `fan_motor_driver.c` | `ioctl` | 모터 풍량 레벨(0~4) 설정, 기동 시 200ms 킥스타트(100% 듀티) 적용 |
| `/dev/fan_led` | `fan_led_driver.c` | `ioctl` | 풍량 레벨(0~4)에 따른 LED 단계 점등 제어 |
| `/dev/bmp180` | `fan_bmp180_driver.c` | `read` | 0.1℃ 단위 정수 문자열 출력 (예: `"246\n"` = 24.6℃) |
| `/dev/lcd1602` | `fan_lcd_driver.c` | `pwrite`, `ioctl` | PCF8574 기반 HD44780 제어, 영역별 부분 갱신 및 백라이트 제어 |

---

## ✨ 핵심 기능 (Key Features)

### 1. FSM 기반 통합 제어
* **Power FSM**: 전원 ON/OFF 시 액추에이터 즉시 정지, LED 소등, LCD 지우기 및 백라이트 OFF.
* **Mode FSM**:
  * **수동(MAN) 모드**: 풍량 엔코더로 0~4단계 직접 조작.
  * **자동(AUTO) 모드**: BMP180 온도를 1초마다 읽어 단계 자동 설정 (0.5℃ 히스테리시스 및 3초 연속 유지 조건으로 떨림 방지).
* **Timer FSM**:
  * 우회전 시 10분 증가, 좌회전 시 10분 감소 (최대 12시간).
  * 설정 중 10초 무입력 또는 버튼 클릭 시 자동 확정 및 1초 카운트다운 시작.
* **Swing FSM**: 회전 토글 및 각도 유지 제어.

### 2. LCD 16x2 부분 갱신 (Flicker-Free)
화면 전체를 지우지 않고 변경된 구역만 `pwrite(offset)`을 통해 갱신합니다.
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
│   ├── src/
│   │   ├── bmp180.c            # 온도 센서 모듈 구현
│   │   ├── display.c           # LCD 출력 모듈 구현
│   │   ├── input.c             # 버튼/엔코더 입력 처리 모듈
│   │   ├── led.c               # LED 제어 모듈
│   │   ├── main.c              # 메인 이벤트 루프 및 FSM 제어
│   │   ├── motor.c             # 모터 구동 인터페이스 모듈
│   │   └── timer.c             # 타이머 로직 및 시간 계산
│   └── Makefile                # 애플리케이션 빌드 Makefile
├── driver/
│   ├── fan_bmp180_driver.c     # BMP180 I2C 커널 드라이버
│   ├── fan_input_driver.c      # GPIO IRQ & 디바운스 입력 드라이버
│   ├── fan_lcd_driver.c        # HD44780 + PCF8574 LCD I2C 드라이버
│   ├── fan_led_driver.c        # GPIO 배열 LED 드라이버
│   ├── fan_motor_driver.c      # L298N PWM 모터 드라이버
│   └── init.txt                # 드라이버 설정 및 메모
├── dts/
│   └── fan-overlay.dts         # 디바이스 트리 오버레이 소스
├── include/
│   ├── fan_api.h               # 애플리케이션 모듈 인터페이스 헤더
│   └── fan_ioctl.h             # 커널-유저 공간 공용 ioctl 및 이벤트 정의 헤더
├── scripts/
│   ├── load_all.sh             # 전체 커널 모듈 일괄 적재 스크립트
│   └── unload_all.sh           # 전체 커널 모듈 일괄 해제 스크립트
├── .gitignore
└── README.md
```

---

## 🚀 빌드 및 실행 방법 (Quick Start)

### 1. 디바이스 트리 오버레이 적용
```bash
# DTS 오버레이 컴파일
dtc -I dts -O dtb -o fan-overlay.dtbo dts/fan-overlay.dts

# extlinux.conf에 오버레이 등록 후 시스템 재부팅
sudo cp fan-overlay.dtbo /boot/
```

### 2. 드라이버 빌드 및 모듈 적재
`scripts` 폴더의 스크립트를 사용하여 손쉽게 적재/해제할 수 있습니다.
```bash
# 드라이버 모듈 적재
sudo ./scripts/load_all.sh

# (필요 시) 드라이버 모듈 해제
sudo ./scripts/unload_all.sh
```

### 3. 애플리케이션 빌드 및 실행
```bash
# 애플리케이션 디렉터리로 이동 후 빌드
cd app
make

# 제어 프로그램 실행 (루트 권한 필요)
sudo ./main
```