/*
 * Copyright (c) 2023 Kim, KyoungHo <kim.kyoungho@gmail.com>
 * All right reserved.
 *
 * This software is confidential and proprietary information.
 * You shall use it only in accordance with the terms of the
 * license agreement you entered into with the copyright holder.
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sys/queue.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "eventloop.h"

static const char* TAG = "eventloop";

ESP_EVENT_DEFINE_BASE(TASK_EVENTS);
ESP_EVENT_DEFINE_BASE(TIMER_EVENTS);

struct _sc_eventloop;
typedef struct _sc_event {
    struct _sc_eventloop *eventloop;
    esp_event_handler_instance_t instance;

    int id;
    sc_event_callback_t callback;
    void *arg;

    uint32_t interval;
    bool repeat;

    SLIST_ENTRY(_sc_event) next;
} _sc_event_t;

typedef SLIST_HEAD(_sc_events, _sc_event) _sc_events_t;

typedef struct _sc_notify {
    sc_notify_callback_t callback;
    void *arg;
    SLIST_ENTRY(_sc_notify) next;
} _sc_notify_t;

typedef SLIST_HEAD(_sc_notifies, _sc_notify) _sc_notifies_t;

typedef struct _sc_eventloop {
    const char *name;
    esp_event_loop_handle_t handle;

    SemaphoreHandle_t mutex;

    _sc_events_t event_nodes;

    esp_event_handler_instance_t timer_instance;
    _sc_events_t timer_nodes;

    _sc_notifies_t notify_nodes;
} _sc_eventloop_t;

static _sc_event_t *_get_event(_sc_events_t *events, int id)
{
    _sc_event_t *it, *tmp;

    if (SLIST_EMPTY(events)) { return NULL; }

    SLIST_FOREACH_SAFE(it, events, next, tmp) {
        if (it->id == id) { return it; }
    }

    return NULL;
}

static _sc_notify_t *_get_notify(_sc_notifies_t *notifies, sc_notify_callback_t callback)
{
    _sc_notify_t *it, *tmp;

    if (SLIST_EMPTY(notifies)) { return NULL; }

    SLIST_FOREACH_SAFE(it, notifies, next, tmp) {
        if (it->callback == callback) { return it; }
    }

    return NULL;
}

static void _eventloop_iteration_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) handler_args;
    _sc_event_t *event = NULL;

    ESP_GOTO_ON_FALSE(loop, ESP_ERR_INVALID_ARG, end, TAG, "invalid eventloop handle");

    if (base == TIMER_EVENTS) {
        id = *((int *)event_data);
        event = _get_event(&(loop->timer_nodes), id);
        ESP_GOTO_ON_FALSE(event, ESP_ERR_NOT_FOUND, end, TAG, "no event with timer id %lu", id);
    } else {
        event = _get_event(&(loop->event_nodes), id);
        ESP_GOTO_ON_FALSE(event, ESP_ERR_NOT_FOUND, end, TAG, "no event with event id %lu", id);
    }

    if (event->callback) {
            event->callback(id, event->arg, event_data);
        if (base == TIMER_EVENTS) {
            event = _get_event(&(loop->timer_nodes), id);
            if (event && event->repeat) {
                if (!esp_timer_is_active(event->instance))
                    esp_timer_start_once(event->instance, event->interval * 1000);
            }
        }
    }

end:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "eventloop iteration handler failed with %d", ret);
    };
}

static void _eventloop_timer_callback(void* arg)
{
    _sc_event_t *event = (_sc_event_t *) arg;
    ESP_RETURN_ON_FALSE(event && event->eventloop, ,TAG, "invalid event handle");

    _sc_eventloop_t *loop = event->eventloop;

    xSemaphoreTake(loop->mutex, portMAX_DELAY);
    int id = event->id;
    xSemaphoreGive(loop->mutex);

    /* Same reason as sc_eventloop_post_event: never block on the queue while
     * holding the mutex a loop handler may be waiting for. */
    esp_event_post_to(loop->handle, TIMER_EVENTS, 0, &id, sizeof(id), portMAX_DELAY);
}

sc_eventloop_t sc_eventloop_create(const char *name, int maxqueue, int stacksize)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = NULL;

    loop = (_sc_eventloop_t *) calloc(1, sizeof(_sc_eventloop_t));
    ESP_RETURN_ON_FALSE(loop, NULL, TAG, "eventloop allocation failed");

    loop->mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(loop->mutex, ESP_FAIL, err, TAG, "mutex creation failed");

    esp_event_loop_args_t args = {
        .queue_size = maxqueue,
        .task_name = name,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = stacksize,
        .task_core_id = tskNO_AFFINITY};

    loop->name = name;
    SLIST_INIT(&(loop->event_nodes));
    SLIST_INIT(&(loop->timer_nodes));
    SLIST_INIT(&(loop->notify_nodes));

    ESP_LOGI(TAG, "create eventloop %s, max queue %d, stack size %d\n", name, maxqueue, stacksize);
    ESP_GOTO_ON_FALSE(esp_event_loop_create(&args, &loop->handle) == ESP_OK, ESP_FAIL, err, TAG, "eventloop creation failed");
    ESP_GOTO_ON_FALSE(esp_event_handler_instance_register_with(loop->handle, TIMER_EVENTS, 0, _eventloop_iteration_handler, loop, &loop->timer_instance) == ESP_OK, ESP_FAIL, err, TAG, "eventloop timer handler registration failed");

    return loop;

err:
    ESP_LOGE(TAG, "eventloop creation failed with %d", ret);

    if (loop) {
        if (loop->handle) esp_event_loop_delete(loop->handle);
        free(loop);
    }

    return NULL;
}

esp_err_t sc_eventloop_destroy(sc_eventloop_t handle)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) handle;

    if (loop) {

        if (loop->handle) {
            _sc_event_t *it, *tmp;

            if (loop->mutex) xSemaphoreTake(loop->mutex, portMAX_DELAY);

            SLIST_FOREACH_SAFE(it, &(loop->event_nodes), next, tmp) {
                if (it->instance) {
                    esp_event_handler_instance_unregister_with(loop->handle, TASK_EVENTS, it->id, it->instance);
                }

                SLIST_REMOVE(&(loop->event_nodes), it, _sc_event, next);
                free(it);
            }

            if (loop->timer_instance) {
                esp_event_handler_instance_unregister_with(loop->handle, TIMER_EVENTS, 0, loop->timer_instance);
            }

            SLIST_FOREACH_SAFE(it, &(loop->timer_nodes), next, tmp) {
                if (it->instance) {
                    esp_timer_delete(it->instance);
                }

                SLIST_REMOVE(&(loop->timer_nodes), it, _sc_event, next);
                free(it);
            }

            _sc_notify_t *nit, *ntmp;
            SLIST_FOREACH_SAFE(nit, &(loop->notify_nodes), next, ntmp) {
                SLIST_REMOVE(&(loop->notify_nodes), nit, _sc_notify, next);
                free(nit);
            }

            esp_event_loop_delete(loop->handle);

            if (loop->mutex) xSemaphoreGive(loop->mutex);
        }

        if (loop->mutex) vSemaphoreDelete(loop->mutex);

        free(loop);
    }

    return ESP_OK;
}

const char *sc_eventloop_get_name(sc_eventloop_t eventloop)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    if (loop) {
        return loop->name;
    }

    return NULL;
}

static esp_err_t _eventloop_del_event(sc_eventloop_t eventloop, int id);

esp_err_t sc_eventloop_add_event(sc_eventloop_t eventloop, int id, sc_event_callback_t callback, void *arg)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    xSemaphoreTake(loop->mutex, portMAX_DELAY);

    _sc_event_t *event = _get_event(&loop->event_nodes, id);
    if (event) _eventloop_del_event(eventloop, id);

    event = (_sc_event_t *) calloc(1, sizeof(_sc_event_t));
    ESP_GOTO_ON_FALSE(event, ESP_ERR_NO_MEM, end, TAG, "failed to alloc event %d", id);

    event->id = id;
    event->callback = callback;
    event->arg = arg;
    event->eventloop = loop;

    ESP_GOTO_ON_ERROR(esp_event_handler_instance_register_with(loop->handle, TASK_EVENTS, id, _eventloop_iteration_handler, loop, &event->instance), end, TAG, "failed to register event %d", id);
    ESP_GOTO_ON_FALSE(event->instance, ESP_ERR_NO_MEM, end, TAG, "failed to register event %d", id);

    SLIST_INSERT_HEAD(&(loop->event_nodes), event, next);

end:

    if (ret != ESP_OK && event) {
        if (event->instance)
            esp_event_handler_instance_unregister_with(loop->handle, TASK_EVENTS, id, event->instance);
        free(event);
    }

    xSemaphoreGive(loop->mutex);

    return ret;
}

esp_err_t _eventloop_del_event(sc_eventloop_t eventloop, int id)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    _sc_event_t *event = _get_event(&loop->event_nodes, id);
    ESP_RETURN_ON_FALSE(event && event->instance, ESP_ERR_NOT_FOUND, TAG, "evnet %d is not found", id);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_unregister_with(loop->handle, TASK_EVENTS, id, event->instance));

    SLIST_REMOVE(&(loop->event_nodes), event, _sc_event, next);
    free(event);

    return ESP_OK;
}

esp_err_t sc_eventloop_del_event(sc_eventloop_t eventloop, int id)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    xSemaphoreTake(loop->mutex, portMAX_DELAY);
    ret = _eventloop_del_event(eventloop, id);
    xSemaphoreGive(loop->mutex);

    return ret;
}

esp_err_t sc_eventloop_post_event(sc_eventloop_t eventloop, int id, const void *data, size_t size)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    xSemaphoreTake(loop->mutex, portMAX_DELAY);

    _sc_event_t *event = _get_event(&loop->event_nodes, id);
    bool registered = event && event->instance;

    /* Post outside the mutex: a full queue blocks here until the loop task
     * drains it, and loop handlers may need the mutex (add_timer) to do so. */
    xSemaphoreGive(loop->mutex);

    ESP_RETURN_ON_FALSE(registered, ESP_ERR_NOT_FOUND, TAG, "evnet %d is not found", id);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_post_to(loop->handle, TASK_EVENTS, id, data, size, portMAX_DELAY));

    return ret;
}

esp_err_t sc_eventloop_post_event_from_isr(sc_eventloop_t eventloop, int id, const void *data, size_t size)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;
    BaseType_t woken = pdFALSE;

    if (!loop) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = esp_event_isr_post_to(loop->handle, TASK_EVENTS, id, data, size, &woken);
    if (woken) portYIELD_FROM_ISR();

    return ret;
}

static esp_err_t _eventloop_del_timer(_sc_event_t *event);

esp_err_t sc_eventloop_add_timer(sc_eventloop_t eventloop, int id, sc_event_callback_t callback, void *arg, uint32_t timeout, bool repeat)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    xSemaphoreTake(loop->mutex, portMAX_DELAY);

    _sc_event_t *event = _get_event(&loop->timer_nodes, id);
    if (event) {
        SLIST_REMOVE(&(loop->timer_nodes), event, _sc_event, next);
        _eventloop_del_timer(event);
    }

    event = (_sc_event_t *) calloc(1, sizeof(_sc_event_t));
    ESP_GOTO_ON_FALSE(event, ESP_ERR_NO_MEM, end, TAG, "failed to alloc event %d", id);

    event->id = id;
    event->callback = callback;
    event->arg = arg;
    event->eventloop = loop;
    event->interval = timeout;
    event->repeat = repeat;

    esp_timer_create_args_t timer_args = {
        .arg = event,
        .callback = &_eventloop_timer_callback,
    };

    ESP_GOTO_ON_ERROR(esp_timer_create(&timer_args, (esp_timer_handle_t *)&event->instance), end, TAG, "failed to create timer %d", id);
    ESP_GOTO_ON_FALSE(event->instance, ESP_ERR_NO_MEM, end, TAG, "failed to create timer %d", id);
    ESP_GOTO_ON_ERROR(esp_timer_start_once((esp_timer_handle_t)event->instance, timeout * 1000), end, TAG, "failed to start timer %d", id);

    SLIST_INSERT_HEAD(&(loop->timer_nodes), event, next);

end:
    if (ret != ESP_OK) {
        if (event) _eventloop_del_timer(event);
    }

    xSemaphoreGive(loop->mutex);

    return ret;
}

esp_err_t _eventloop_del_timer(_sc_event_t *event)
{
    if (event) {
        if (event->instance) {
            esp_timer_stop((esp_timer_handle_t)event->instance);
            esp_timer_delete((esp_timer_handle_t)event->instance);
        }

        free(event);
    }

    return 0;
}

esp_err_t sc_eventloop_del_timer(sc_eventloop_t eventloop, int id)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");


    xSemaphoreTake(loop->mutex, portMAX_DELAY);

    _sc_event_t *event = _get_event(&loop->timer_nodes, id);
    if (event) {
        SLIST_REMOVE(&(loop->timer_nodes), event, _sc_event, next);
        ret = _eventloop_del_timer(event);
    }

    xSemaphoreGive(loop->mutex);

    return ret;
}

esp_err_t sc_eventloop_notify_register(sc_eventloop_t eventloop, sc_notify_callback_t callback, void *arg)
{
    esp_err_t ret = ESP_OK;
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    _sc_notify_t *notify = _get_notify(&loop->notify_nodes, callback);
    if (notify) sc_eventloop_notify_unregister(eventloop, callback);

    notify = (_sc_notify_t *) calloc(1, sizeof(_sc_notify_t));
    ESP_GOTO_ON_FALSE(notify, ESP_ERR_NO_MEM, err, TAG, "failed to alloc notify");

    notify->callback = callback;
    notify->arg = arg;

    SLIST_INSERT_HEAD(&(loop->notify_nodes), notify, next);

    return ret;

err:
    if (notify) free(notify);
    return ret;
}

esp_err_t sc_eventloop_notify_unregister(sc_eventloop_t eventloop, sc_notify_callback_t callback)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    _sc_notify_t *notify = _get_notify(&loop->notify_nodes, callback);
    ESP_RETURN_ON_FALSE(notify, ESP_ERR_NOT_FOUND, TAG, "callback %p is not found", callback);

    SLIST_REMOVE(&(loop->notify_nodes), notify, _sc_notify, next);
    free(notify);

    return 0;
}

esp_err_t sc_eventloop_notify(sc_eventloop_t eventloop, int type)
{
    _sc_eventloop_t *loop = (_sc_eventloop_t *) eventloop;
    _sc_notify_t *it, *tmp;

    ESP_RETURN_ON_FALSE(loop, ESP_ERR_INVALID_ARG, TAG, "eventloop handle is null");

    SLIST_FOREACH_SAFE(it, &(loop->notify_nodes), next, tmp) {
        if (it->callback) it->callback(type, it->arg);
    }

    return ESP_OK;
}
