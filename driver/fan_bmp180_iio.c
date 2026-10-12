#include <linux/kernel.h>
#include <linux/iio/iio.h>

#include "fan_bmp180.h"

/*
 * IIO 읽기 콜백
 *
 * 함수의 return 값은 IIO 값의 표현 방식 또는 오류 코드
 * 실제 측정값은 val / val2를 통해 IIO에 전달
 */
static int fan_bmp180_read_raw(
    struct iio_dev *indio_dev,
    const struct iio_chan_spec *chan,
    int *val, int *val2, long mask)
{
    struct fan_bmp180_data *data = iio_priv(indio_dev);
    struct fan_bmp180_measurement result;
    int ret;

    if (mask != IIO_CHAN_INFO_PROCESSED)
        return -EINVAL;

    if (chan->type != IIO_TEMP && chan->type != IIO_PRESSURE)
        return -EINVAL;

    ret = fan_bmp180_get_measurement(data, &result);
    if (ret)
        return ret;

    switch (chan->type) {
    case IIO_TEMP:
        /* IIO 온도 입력의 표준 단위: m°C */
        *val = result.temperature_mc;
        *val2 = 0;
        return IIO_VAL_INT;

    case IIO_PRESSURE:
        /* IIO 기압 입력의 표준 단위: kPa */
        *val = result.pressure_pa / 1000;
        *val2 = (result.pressure_pa % 1000) * 1000;
        return IIO_VAL_INT_PLUS_MICRO;

    default:
        return -EINVAL;
    }
}

static const struct iio_info fan_bmp180_iio_info = {
    .read_raw = fan_bmp180_read_raw,
};

static const struct iio_chan_spec fan_bmp180_channels[] = {
    {
        .type = IIO_TEMP,
        .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
    },
    {
        .type = IIO_PRESSURE,
        .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
    },
};

int fan_bmp180_iio_register(struct device *dev, struct iio_dev *indio_dev)
{
    indio_dev->name = "fan_bmp180";
    indio_dev->dev.parent = dev;
    indio_dev->info = &fan_bmp180_iio_info;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = fan_bmp180_channels;
    indio_dev->num_channels = ARRAY_SIZE(fan_bmp180_channels);

    return devm_iio_device_register(dev, indio_dev);
}
