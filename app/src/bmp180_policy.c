// 온도는 0.1도 단위 정수 (264 = 26.4도)
#define TEMP_LOW     260
#define TEMP_MID     290
#define TEMP_HIGH    320
#define TEMP_MAX     350

// 온도에 따른 풍량. 26.0도 이하는 0단계(모터 정지)
void fan_temp_policy(int *target_level, int current_temp) {
    if (current_temp >= TEMP_MAX)       *target_level = 4;
    else if (current_temp > TEMP_HIGH)  *target_level = 3;
    else if (current_temp > TEMP_MID)   *target_level = 2;
    else if (current_temp > TEMP_LOW)   *target_level = 1;
    else                                *target_level = 0;
}