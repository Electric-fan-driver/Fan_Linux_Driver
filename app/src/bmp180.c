#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fan_api.h"

#define IIO_ROOT "/sys/bus/iio/devices"
#define BMP180_NAME "fan_bmp180"
#define BMP180_PATH_SIZE 512

static char bmp180_directory[BMP180_PATH_SIZE];

int bmp180_open(void)
{
    DIR *dir;
    struct dirent *entry;
    char candidate[BMP180_PATH_SIZE];
    char path[BMP180_PATH_SIZE];
    char name[128];
    FILE *file;
    int length;
    int ret = -ENODEV;

    bmp180_directory[0] = '\0';
    dir = opendir(IIO_ROOT);
    if (!dir)
        return -errno;

    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno)
                ret = -errno;
            break;
        }

        if (strncmp(entry->d_name, "iio:device", 10) != 0)
            continue;

        length = snprintf(candidate, sizeof(candidate), "%s/%s",
                          IIO_ROOT, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof(candidate))
            continue;

        length = snprintf(path, sizeof(path), "%s/name", candidate);
        if (length < 0 || (size_t)length >= sizeof(path))
            continue;

        file = fopen(path, "r");
        if (!file)
            continue;

        if (!fgets(name, sizeof(name), file)) {
            fclose(file);
            continue;
        }
        fclose(file);
        name[strcspn(name, "\r\n")] = '\0';

        if (strcmp(name, BMP180_NAME) != 0)
            continue;

        if (ret == 0) {
            ret = -EEXIST;
            break;
        }

        strcpy(bmp180_directory, candidate);
        ret = 0;
    }

    closedir(dir);
    if (ret)
        bmp180_directory[0] = '\0';

    return ret;
}

void bmp180_close(void)
{
    bmp180_directory[0] = '\0';
}

/* 매번 파일을 다시 열어 IIO 드라이버에 새 측정을 요청한다. */
static int bmp180_read_value(const char *attribute, double *value)
{
    char path[BMP180_PATH_SIZE];
    FILE *file;
    double measured;
    int length;
    int scanned;
    int ret;

    if (!bmp180_directory[0])
        return -ENODEV;

    length = snprintf(path, sizeof(path), "%s/%s",
                      bmp180_directory, attribute);
    if (length < 0 || (size_t)length >= sizeof(path))
        return -ENAMETOOLONG;

    file = fopen(path, "r");
    if (!file)
        return -errno;

    errno = 0;
    scanned = fscanf(file, "%lf", &measured);
    ret = scanned == 1 ? 0 : -(errno ? errno : EIO);
    if (fclose(file) != 0 && !ret)
        ret = -errno;
    if (ret)
        return ret;
    if (!isfinite(measured))
        return -ERANGE;

    *value = measured;
    return 0;
}

int bmp180_get_temperature(int *temp_x10)
{
    double temperature_mc;
    int ret;

    if (!temp_x10)
        return -EINVAL;

    ret = bmp180_read_value("in_temp_input", &temperature_mc);
    if (ret)
        return ret;
    if (temperature_mc / 100.0 < INT_MIN ||
        temperature_mc / 100.0 > INT_MAX)
        return -ERANGE;

    *temp_x10 = (int)(temperature_mc / 100.0);
    return 0;
}

int bmp180_get_measurement(int *temp_x10, int *pressure_pa)
{
    double pressure_kpa;
    double pressure;
    int temperature;
    int ret;

    if (!temp_x10 || !pressure_pa)
        return -EINVAL;

    ret = bmp180_get_temperature(&temperature);
    if (ret)
        return ret;

    ret = bmp180_read_value("in_pressure_input", &pressure_kpa);
    if (ret)
        return ret;

    pressure = pressure_kpa * 1000.0;
    if (pressure < 0 || pressure > INT_MAX - 0.5)
        return -ERANGE;

    *temp_x10 = temperature;
    *pressure_pa = (int)(pressure + 0.5);
    return 0;
}
