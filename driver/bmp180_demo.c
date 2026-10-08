#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

// 일치하는 name이 있으면 probe 호출
static int bmp180_demo_probe(struct i2c_client *client)
{
    dev_info(&client->dev,
             "probe called: bus=%d, address=0x%02x\n",
             client->adapter->nr, client->addr);

    return 0;
}

// DT 의 compatible 속성과 비교
static const struct of_device_id bmp180_demo_of_match[] = {
    { .compatible = "bosch,bmp180" },
    { }
};
MODULE_DEVICE_TABLE(of, bmp180_demo_of_match);

// 드라이버 이름 및 연결 함수 등록
static struct i2c_driver bmp180_demo_driver = {
    .driver = {
        .name = "bmp180_demo",
        .of_match_table = bmp180_demo_of_match,
    },
    .probe_new = bmp180_demo_probe,
};

module_i2c_driver(bmp180_demo_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMP180 learning driver");