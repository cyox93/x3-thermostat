#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "board.h"
#include "plc.h"

/*
 * Symbol timing in µs, from DOUT_BUF measurements (docs/plc-protocol.md §2).
 * A cell starts on the carrier rising edge; the carrier on-time is the value.
 * DOUT_BUF moves on a 8 µs grid (4 carrier periods), so these are the
 * measured means. The on-times we transmit assume the detector adds no
 * length — unverified until our own bursts are measured on the bus.
 */
#define _CELL_US            1017
#define _START_ON_US        900
#define _HEAD_ON_US         72
#define _BIT0_ON_US         296
#define _BIT1_ON_US         600
#define _END_ON_US          32
#define _FRAME_PERIOD_US    12140
#define _FRAME_DATA_BITS    8
/* START + head pulse + 8 bits + END */
#define _FRAME_SYMBOLS      (2 + _FRAME_DATA_BITS + 1)

/* RX decision points (docs/plc-protocol.md §3) */
#define _RX_START_MIN_US    800     /* longer = START */
#define _RX_DATA_MIN_US     200     /* shorter = head / END pulse, not data */
#define _RX_BIT1_MIN_US     450     /* longer = 1 */

/* The receive is re-armed from the done ISR, and a frame that starts before
 * that loses its START. Ending a receive at 1.5 ms of idle (between the 944 µs
 * in-frame low and the 1.94 ms gap between frames) left only 0.44 ms for that,
 * and the bench showed bursts starting mid-START. 5 ms keeps a whole message
 * in one receive (the decoder splits frames at each START) and re-arms in the
 * gap between messages instead. */
#define _RX_IDLE_NS         (5000 * 1000)
#define _ECHO_SLACK_US      3000    /* last carrier ends ~1.9 ms before the send does */
/* Hardware filter caps near 3 µs; the shortest real pulse is ~28 µs */
#define _RX_GLITCH_NS       2000
/* One frame is 11 symbols. Devices answer right after the previous message
 * ends, so a poll, a request and two answers can arrive as one unbroken
 * receive (17 frames seen on the bench). The S3 streams the RX memory into
 * this buffer ping-pong, so only the buffer bounds a receive: 1024 symbols
 * = 93 frames. (DMA was tried: it dropped the last frame of long bursts.) */
#define _RX_SYMBOLS         1024
#define _RX_MEM_SYMBOLS     96      /* 2 blocks: 48-symbol halves between copies */

#define _RMT_RESOLUTION_HZ  (1000 * 1000)

static const char *TAG = "plc";

static rmt_channel_handle_t s_rx_chan;
static rmt_channel_handle_t s_tx_chan;
static rmt_encoder_handle_t s_copy_encoder;

/* Two buffers: the next receive is armed before the last one is decoded */
static rmt_symbol_word_t s_rx_buf[2][_RX_SYMBOLS];
static int s_rx_idx;
static rmt_symbol_word_t s_tx_buf[SC_PLC_SEND_MAX * _FRAME_SYMBOLS];
static volatile bool s_tx_busy;
static volatile int64_t s_tx_done_us;

/* Last burst that did not decode, as level << 15 | µs (see sc_plc_get_error_burst) */
static uint16_t s_err_burst[SC_PLC_BURST_MAX];
static volatile size_t s_err_len;

static sc_plc_callback_t s_callback;
static void *s_callback_arg;

static const rmt_receive_config_t s_rx_cfg = {
    .signal_range_min_ns = _RX_GLITCH_NS,
    .signal_range_max_ns = _RX_IDLE_NS,
};

static void _notify(sc_plc_event_t event, int value)
{
    if (s_callback) {
        s_callback(event, value, s_callback_arg);
    }
}

static void _save_burst(const rmt_symbol_word_t *sym, size_t num)
{
    size_t n = 0;
    for (size_t i = 0; i < num * 2 && n < sizeof(s_err_burst) / sizeof(s_err_burst[0]); i++) {
        uint32_t level = (i & 1) ? sym[i / 2].level1 : sym[i / 2].level0;
        uint32_t us = (i & 1) ? sym[i / 2].duration1 : sym[i / 2].duration0;
        if (!us) break;
        s_err_burst[n++] = (level << 15) | (us & 0x7fff);
    }
    s_err_len = n;
}

/* Decode one burst — one or more frames — and report each. Only carrier-on
 * lengths matter; the lows are the cell remainder and carry nothing. A burst
 * with any bad frame is kept for sc_plc_get_error_burst. */
static void _decode(const rmt_symbol_word_t *sym, size_t num, bool echo)
{
    bool started = false;
    bool saved = false;
    int orphans = 0;        /* data pulses with no START before them */
    int bits = 0;
    int value = 0;

    for (size_t i = 0; i <= num * 2; i++) {
        uint32_t level = 0;
        uint32_t us = 0;
        if (i < num * 2) {
            level = (i & 1) ? sym[i / 2].level1 : sym[i / 2].level0;
            us = (i & 1) ? sym[i / 2].duration1 : sym[i / 2].duration0;
            if (!level || !us) continue;
        }

        /* A START, or the end of the burst, closes the frame before it */
        bool end = (i == num * 2);
        if (end || us > _RX_START_MIN_US) {
            if (started && bits == _FRAME_DATA_BITS) {
                _notify(SC_PLC_EVENT_RX, value | (echo ? SC_PLC_RX_ECHO : 0));
            } else if (started || orphans || end) {
                if (!saved) {
                    _save_burst(sym, num);
                    saved = true;
                }
                _notify(SC_PLC_EVENT_RX_ERROR, started ? SC_PLC_RX_ERR_BITS : SC_PLC_RX_ERR_NO_START);
            }
            started = !end;
            orphans = 0;
            bits = 0;
            value = 0;
        } else if (started && us >= _RX_DATA_MIN_US) {
            /* First bit on the wire is the LSB (docs/plc-protocol.md §4) */
            if (bits < _FRAME_DATA_BITS) value |= (us > _RX_BIT1_MIN_US) << bits;
            bits++;
        } else if (!started && us >= _RX_DATA_MIN_US) {
            orphans++;
        }
    }
}

static bool _rx_done_cb(rmt_channel_handle_t chan, const rmt_rx_done_event_data_t *edata, void *arg)
{
    /* Re-arm first — whatever starts before this runs is lost */
    s_rx_idx ^= 1;
    rmt_receive(s_rx_chan, s_rx_buf[s_rx_idx], sizeof(s_rx_buf[0]), &s_rx_cfg);

    /* Our own send comes back as one burst that closes only after the idle
     * time, when the send has already finished: a burst that closes within
     * idle + one frame gap of the send's end is its echo */
    bool echo = s_tx_busy || esp_timer_get_time() - s_tx_done_us < _RX_IDLE_NS / 1000 + _ECHO_SLACK_US;
    _decode(edata->received_symbols, edata->num_symbols, echo);
    return false;
}

static bool _tx_done_cb(rmt_channel_handle_t chan, const rmt_tx_done_event_data_t *edata, void *arg)
{
    s_tx_done_us = esp_timer_get_time();
    s_tx_busy = false;
    _notify(SC_PLC_EVENT_TX_DONE, 0);
    return false;
}

static inline rmt_symbol_word_t _cell(uint32_t on_us, uint32_t off_us)
{
    return (rmt_symbol_word_t) {
        .level0 = 1, .duration0 = on_us,
        .level1 = 0, .duration1 = off_us,
    };
}

/* One frame into 11 symbols. The END cell's low stretches to the frame
 * period, so frames sent back to back keep the bus spacing. */
static void _encode_frame(rmt_symbol_word_t *out, uint8_t byte)
{
    int n = 0;

    out[n++] = _cell(_START_ON_US, _CELL_US - _START_ON_US);
    out[n++] = _cell(_HEAD_ON_US, _CELL_US - _HEAD_ON_US);
    for (int b = 0; b < _FRAME_DATA_BITS; b++) {
        uint32_t on = (byte >> b) & 1 ? _BIT1_ON_US : _BIT0_ON_US;
        out[n++] = _cell(on, _CELL_US - on);
    }
    out[n++] = _cell(_END_ON_US, _FRAME_PERIOD_US - (_FRAME_SYMBOLS - 1) * _CELL_US - _END_ON_US);
}

esp_err_t sc_plc_send(const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(data && len > 0 && len <= SC_PLC_SEND_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid length %u", (unsigned)len);
    ESP_RETURN_ON_FALSE(!s_tx_busy, ESP_ERR_INVALID_STATE, TAG, "send in progress");

    for (size_t i = 0; i < len; i++) {
        _encode_frame(&s_tx_buf[i * _FRAME_SYMBOLS], data[i]);
    }

    /* Logical 1 = carrier on; the output is inverted so that is /SCTL LOW.
     * eot_level 0 = /SCTL released = receive. */
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags.eot_level = 0,
    };
    s_tx_busy = true;
    esp_err_t ret = rmt_transmit(s_tx_chan, s_copy_encoder, s_tx_buf,
                                 len * _FRAME_SYMBOLS * sizeof(rmt_symbol_word_t), &tx_cfg);
    if (ret != ESP_OK) {
        s_tx_busy = false;
        ESP_LOGE(TAG, "transmit failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

size_t sc_plc_get_error_burst(uint16_t *buf, size_t max)
{
    size_t n = s_err_len;
    if (n > max) n = max;
    memcpy(buf, s_err_burst, n * sizeof(buf[0]));
    return n;
}

bool sc_plc_tx_busy(void)
{
    return s_tx_busy;
}

bool sc_plc_carrier(void)
{
    return gpio_get_level(BOARD_PLC_DOUT_PIN);
}

esp_err_t sc_plc_register_callback(sc_plc_callback_t callback, void *arg)
{
    s_callback_arg = arg;
    s_callback = callback;
    return ESP_OK;
}

static esp_err_t _tx_init(void)
{
    /* /SCTL has a 10 kΩ pull-up to +5 V inside the module: open-drain only.
     * Set it before the RMT takes the pin so it never drives HIGH. */
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << BOARD_PLC_SCTL_PIN,
        .mode = GPIO_MODE_OUTPUT_OD,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_PLC_SCTL_PIN, 1), TAG, "sctl level failed");
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "sctl config failed");

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = BOARD_PLC_SCTL_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = _RMT_RESOLUTION_HZ,
        .mem_block_symbols = 48,
        .trans_queue_depth = 1,
        .flags.invert_out = 1,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_cfg, &s_tx_chan), TAG, "new tx channel failed");
    /* The RMT only reroutes the pad signal; make sure it stayed open-drain */
    ESP_RETURN_ON_ERROR(gpio_od_enable(BOARD_PLC_SCTL_PIN), TAG, "sctl od failed");

    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&enc_cfg, &s_copy_encoder), TAG, "new encoder failed");

    rmt_tx_event_callbacks_t cbs = {
        .on_trans_done = _tx_done_cb,
    };
    ESP_RETURN_ON_ERROR(rmt_tx_register_event_callbacks(s_tx_chan, &cbs, NULL), TAG, "tx callback failed");
    return rmt_enable(s_tx_chan);
}

static esp_err_t _rx_init(void)
{
    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = BOARD_PLC_DOUT_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = _RMT_RESOLUTION_HZ,
        .mem_block_symbols = _RX_MEM_SYMBOLS,
    };
    ESP_RETURN_ON_ERROR(rmt_new_rx_channel(&rx_cfg, &s_rx_chan), TAG, "new rx channel failed");

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = _rx_done_cb,
    };
    ESP_RETURN_ON_ERROR(rmt_rx_register_event_callbacks(s_rx_chan, &cbs, NULL), TAG, "rx callback failed");
    ESP_RETURN_ON_ERROR(rmt_enable(s_rx_chan), TAG, "rx enable failed");
    return rmt_receive(s_rx_chan, s_rx_buf[s_rx_idx], sizeof(s_rx_buf[0]), &s_rx_cfg);
}

esp_err_t sc_plc_init(void)
{
    ESP_RETURN_ON_ERROR(_tx_init(), TAG, "tx init failed");
    ESP_RETURN_ON_ERROR(_rx_init(), TAG, "rx init failed");

    ESP_LOGI(TAG, "PLC initialized (SCTL=GPIO%d, DOUT=GPIO%d)", BOARD_PLC_SCTL_PIN, BOARD_PLC_DOUT_PIN);
    return ESP_OK;
}
