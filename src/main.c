// Copyright (c) 2016, Intel Corporation.

// Zephyr includes
#include <zephyr.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#include <string.h>

// JerryScript includes
#include "jerry.h"
#include "jerry-api.h"

// ZJS includes
#include "zjs_gpio.h"
#include "zjs_timers.h"
#include "zjs_util.h"

// local includes
#include "script.h"

void main(int argc, char *argv[])
{
    jerry_api_object_t *err_obj_p = NULL;
    jerry_api_value_t res;

    jerry_init(JERRY_FLAG_EMPTY);

    jerry_api_object_t *global_obj = jerry_api_get_global();

    zjs_gpio_init(global_obj);
    zjs_timers_init();

    size_t len = strlen((char *) script);

    if (!jerry_parse((jerry_api_char_t *) script, len, &err_obj_p)) {
        PRINT("JerryScript: cannot parse javascript\n");
        return;
    }

    if (jerry_run(&res) != JERRY_COMPLETION_CODE_OK) {
        PRINT("JerrySript: cannot run javascript\n");
        return;
    }

    while (1) {
        if (zjs_timers_process_events() == false)
            break;
    }
}
