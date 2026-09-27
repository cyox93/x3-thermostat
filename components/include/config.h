/*
 * Copyright (c) 2023 Kim, KyoungHo <kim.kyoungho@gmail.com>
 * All right reserved.
 *
 * This software is confidential and proprietary information.
 * You shall use it only in accordance with the terms of the
 * license agreement you entered into with the copyright holder.
 */

#pragma once
#include <stddef.h>
#include "esp_err.h"

esp_err_t sc_config_init(void);
esp_err_t sc_config_set(const char *key, const char *value);
esp_err_t sc_config_get(const char *key, char *str, size_t len);
