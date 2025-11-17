/*
 * Copyright (c) 2025 Golioth
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_weather, LOG_LEVEL_DBG);

struct weather_data
{
    struct sensor_value tem;
    struct sensor_value pre;
    struct sensor_value hum;
};

#define ERROR_VAL1 999
#define ERROR_VAL2 999999
static const struct sensor_value reading_error = {.val1 = ERROR_VAL1, .val2 = ERROR_VAL2};

/* Global to hold BME280 readings; updated at 1 Hz by thread */
struct weather_data _latest_weather_data = {
    .tem.val1 = ERROR_VAL1,
    .tem.val2 = ERROR_VAL2,
    .pre.val1 = ERROR_VAL1,
    .pre.val2 = ERROR_VAL2,
    .hum.val1 = ERROR_VAL1,
    .hum.val2 = ERROR_VAL2,
};

const struct device *weather_dev;

/* Thread reads weather sensor and provides easy access to latest data */
K_MUTEX_DEFINE(weather_mutex);              /* Protect data */
K_SEM_DEFINE(bme280_initialized_sem, 0, 1); /* Wait until sensor is ready */

void weather_sensor_data_fetch(void)
{
    // LOG_DBG("Reading BME280");
    if (!weather_dev)
    {
        _latest_weather_data.tem = reading_error;
        _latest_weather_data.pre = reading_error;
        _latest_weather_data.hum = reading_error;
        return;
    }
    sensor_sample_fetch(weather_dev);
    k_mutex_lock(&weather_mutex, K_FOREVER);

    sensor_channel_get(weather_dev, SENSOR_CHAN_AMBIENT_TEMP, &_latest_weather_data.tem);
    sensor_channel_get(weather_dev, SENSOR_CHAN_PRESS, &_latest_weather_data.pre);
    sensor_channel_get(weather_dev, SENSOR_CHAN_HUMIDITY, &_latest_weather_data.hum);

    k_mutex_unlock(&weather_mutex);
}

#define WEATHER_STACK 1024

extern void weather_sensor_thread(void *d0, void *d1, void *d2)
{
    /* Block until sensor is available */
    k_sem_take(&bme280_initialized_sem, K_FOREVER);
    while (1)
    {
        weather_sensor_data_fetch();
        k_sleep(K_SECONDS(1));
    }
}

K_THREAD_DEFINE(weather_sensor_tid,
                WEATHER_STACK,
                weather_sensor_thread,
                NULL,
                NULL,
                NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO,
                0,
                0);


/*
 * Get a device structure from a devicetree node with compatible
 * "bosch,bme280". (If there are multiple, just pick one.)
 */
static const struct device *get_bme280_device(void)
{
    const struct device *const bme_dev = DEVICE_DT_GET_ANY(bosch_bme280);

    if (bme_dev == NULL)
    {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR("\nError: no device found.");
        return NULL;
    }

    if (!device_is_ready(bme_dev))
    {
        LOG_ERR(
            "Error: Device \"%s\" is not ready; "
            "check the driver initialization logs for errors.",
            bme_dev->name);
        return NULL;
    }

    LOG_DBG("Found device \"%s\", getting sensor data", bme_dev->name);

    /* Give semaphore to signal sensor is ready for reading */
    k_sem_give(&bme280_initialized_sem);
    return bme_dev;
}

void get_latest_weather(char *buf, uint8_t buf_len)
{
    if ((_latest_weather_data.tem.val1 == reading_error.val1)
        && (_latest_weather_data.tem.val2 == reading_error.val2))
    {
        buf[0] = '\0';
    }
    else
    {
        int len = snprintk(buf,
                           buf_len,
                           "{\"temp\":%d.%06d,\"pre\":%d.%06d,\"hum\":%d.%06d}",
                           _latest_weather_data.tem.val1,
                           abs(_latest_weather_data.tem.val2),
                           _latest_weather_data.pre.val1,
                           abs(_latest_weather_data.pre.val2),
                           _latest_weather_data.hum.val1,
                           abs(_latest_weather_data.hum.val2));
        LOG_DBG("JSON: %s", buf);
    }
}


void app_sensors_init(void)
{
    LOG_INF("Initializing BME280");

    weather_dev = get_bme280_device();
    weather_sensor_data_fetch();
}
