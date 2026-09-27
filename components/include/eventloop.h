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
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef void *sc_eventloop_t;
typedef void (*sc_event_callback_t)(int id, void *arg, void *data);
typedef void (*sc_notify_callback_t)(int type, void *arg);

sc_eventloop_t sc_eventloop_create(const char *name, int maxqueue, int stacksize);
esp_err_t sc_eventloop_destroy(sc_eventloop_t eventloop);
const char *sc_eventloop_get_name(sc_eventloop_t eventloop);

esp_err_t sc_eventloop_add_event(sc_eventloop_t eventloop, int id, sc_event_callback_t callback, void *arg);
esp_err_t sc_eventloop_del_event(sc_eventloop_t eventloop, int id);
esp_err_t sc_eventloop_post_event(sc_eventloop_t eventloop, int id, const void *data, size_t size);
/* ISR-safe. data is copied and limited to sizeof(int). An id with no handler is
 * silently dropped (no registration check — that needs the loop mutex). */
esp_err_t sc_eventloop_post_event_from_isr(sc_eventloop_t eventloop, int id, const void *data, size_t size);
esp_err_t sc_eventloop_add_timer(sc_eventloop_t eventloop, int id, sc_event_callback_t callback, void *arg, uint32_t timeout, bool repeat);
esp_err_t sc_eventloop_del_timer(sc_eventloop_t eventloop, int id);

esp_err_t sc_eventloop_notify_register(sc_eventloop_t eventloop, sc_notify_callback_t callback, void *arg);
esp_err_t sc_eventloop_notify_unregister(sc_eventloop_t eventloop, sc_notify_callback_t callback);
esp_err_t sc_eventloop_notify(sc_eventloop_t eventloop, int type);
