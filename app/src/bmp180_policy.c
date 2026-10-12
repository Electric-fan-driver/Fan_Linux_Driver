#define TEMP_LOW     26
#define TEMP_MID     29
#define TEMP_HIGH    32
#define TEMP_MAX     35

// 온도(도)에 따른 풍량. 26도 이하는 0단계(모터 정지)
void fan_temp_policy(int *target_level, int current_temp) {
    if (current_temp >= TEMP_MAX)       *target_level = 4;
    else if (current_temp > TEMP_HIGH)  *target_level = 3;
    else if (current_temp > TEMP_MID)   *target_level = 2;
    else if (current_temp > TEMP_LOW)   *target_level = 1;
    else                                *target_level = 0;
}