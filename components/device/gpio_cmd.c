#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_struct.h"
#include "device.h"

static const char *TAG = "gpio_cmd";

static void print_gpio_status(int pin)
{
    bool out_en = (pin < 32) ? (GPIO.enable & (1U << pin)) : (GPIO.enable1.data & (1U << (pin - 32)));
    uint32_t iomux = REG_READ(IO_MUX_GPIO0_REG + (pin * 4));
    bool in_en = (iomux >> FUN_IE_S) & 1;
    bool pu = (iomux >> FUN_PU_S) & 1;
    bool pd = (iomux >> FUN_PD_S) & 1;
    int level = gpio_get_level(pin);

    const char *dir = out_en ? "output" : (in_en ? "input" : "disabled");
    const char *pull = pu ? "pullup" : (pd ? "pulldown" : "nopull");

    printf("GPIO%d: %s, level=%d, %s\n", pin, dir, level, pull);
}

static struct {
    struct arg_int *pin;
    struct arg_str *dir;
    struct arg_str *opt;
    struct arg_end *end;
} gpio_mode_args;

static int cmd_gpio_mode(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&gpio_mode_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gpio_mode_args.end, argv[0]);
        return 1;
    }

    int pin = gpio_mode_args.pin->ival[0];

    if (!GPIO_IS_VALID_GPIO(pin)) {
        printf("Invalid GPIO: %d\n", pin);
        return 1;
    }

    if (gpio_mode_args.dir->count == 0) {
        print_gpio_status(pin);
        return 0;
    }

    const char *dir = gpio_mode_args.dir->sval[0];

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pin,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (strcmp(dir, "input") == 0) {
        io_conf.mode = GPIO_MODE_INPUT;
        if (gpio_mode_args.opt->count > 0) {
            const char *opt = gpio_mode_args.opt->sval[0];
            if (strcmp(opt, "pullup") == 0) {
                io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            } else if (strcmp(opt, "pulldown") == 0) {
                io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            } else if (strcmp(opt, "nopull") == 0) {
                // no pull
            } else {
                printf("Invalid option: '%s' (use pullup, pulldown, nopull)\n", opt);
                return 1;
            }
        }
    } else if (strcmp(dir, "output") == 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        int level = 0;
        if (gpio_mode_args.opt->count > 0) {
            const char *opt = gpio_mode_args.opt->sval[0];
            if (strcmp(opt, "0") != 0 && strcmp(opt, "1") != 0) {
                printf("Invalid level: '%s' (use 0, 1)\n", opt);
                return 1;
            }
            level = atoi(opt);
        }
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            printf("gpio_config failed: %s\n", esp_err_to_name(err));
            return 1;
        }
        gpio_set_level(pin, level);
        ESP_LOGI(TAG, "GPIO%d output level=%d", pin, level);
        return 0;
    } else {
        printf("Invalid direction: '%s' (use input, output)\n", dir);
        return 1;
    }

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        printf("gpio_config failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    ESP_LOGI(TAG, "GPIO%d %s", pin, dir);
    return 0;
}

static struct {
    struct arg_int *pin;
    struct arg_end *end;
} gpio_get_args;

static int cmd_gpio_get(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&gpio_get_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gpio_get_args.end, argv[0]);
        return 1;
    }

    int pin = gpio_get_args.pin->ival[0];
    if (!GPIO_IS_VALID_GPIO(pin)) {
        printf("Invalid GPIO: %d\n", pin);
        return 1;
    }

    printf("GPIO%d = %d\n", pin, gpio_get_level(pin));
    return 0;
}

static struct {
    struct arg_int *pin;
    struct arg_int *level;
    struct arg_end *end;
} gpio_set_args;

static int cmd_gpio_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&gpio_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gpio_set_args.end, argv[0]);
        return 1;
    }

    int pin = gpio_set_args.pin->ival[0];
    int level = gpio_set_args.level->ival[0];

    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        printf("Invalid output GPIO: %d\n", pin);
        return 1;
    }
    if (level != 0 && level != 1) {
        printf("Invalid level: %d (use 0, 1)\n", level);
        return 1;
    }

    gpio_set_level(pin, level);
    printf("GPIO%d = %d\n", pin, level);
    return 0;
}

esp_err_t sc_gpio_cmd_init(void)
{
    gpio_mode_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO number");
    gpio_mode_args.dir = arg_str0(NULL, NULL, "<dir>", "input, output");
    gpio_mode_args.opt = arg_str0(NULL, NULL, "<opt>", "pullup/pulldown/nopull or 0/1");
    gpio_mode_args.end = arg_end(3);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "gpio_mode", .help = "Show/configure GPIO (gpio_mode <pin> [input pullup|pulldown|nopull] [output 0|1])",
        .func = &cmd_gpio_mode, .argtable = &gpio_mode_args,
    }));

    gpio_get_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO number");
    gpio_get_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "gpio_get", .help = "Read GPIO level",
        .func = &cmd_gpio_get, .argtable = &gpio_get_args,
    }));

    gpio_set_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO number");
    gpio_set_args.level = arg_int1(NULL, NULL, "<0|1>", "output level");
    gpio_set_args.end = arg_end(2);
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "gpio_set", .help = "Set GPIO output level",
        .func = &cmd_gpio_set, .argtable = &gpio_set_args,
    }));

    ESP_LOGI(TAG, "GPIO commands registered");
    return ESP_OK;
}
