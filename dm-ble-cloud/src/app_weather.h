/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WEATHER_H__
#define __APP_WEATHER_H__

void app_sensors_init(void);
void get_latest_weather(char *, uint8_t);

#endif /* __APP_WEATHER_H__ */
