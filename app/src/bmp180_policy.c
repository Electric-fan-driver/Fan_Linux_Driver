#define TEMP_LOW     26
#define TEMP_MID     29
#define TEMP_HIGH    32
#define TEMP_MAX     35

void fan_temp_policy(int *target_level, int current_temp) {
    if (current_temp >= TEMP_HIGH)      *target_level = 4;
    else if (current_temp > TEMP_MID)   *target_level = 3;
    else if (current_temp > TEMP_LOW)   *target_level = 2;
    else                                *target_level = 1;
}