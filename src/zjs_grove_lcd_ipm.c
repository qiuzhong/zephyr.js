// Copyright (c) 2016-2017, Intel Corporation.

#ifdef BUILD_MODULE_GROVE_LCD
#ifndef QEMU_BUILD
#ifndef ZJS_LINUX_BUILD
// Zephyr includes
#include <zephyr.h>
#endif
#include <device.h>
#include <string.h>
#include <display/grove_lcd.h>
#include <misc/util.h>

// ZJS includes
#include "zjs_grove_lcd.h"
#include "zjs_ipm.h"
#include "zjs_util.h"

#define ZJS_GLCD_TIMEOUT_TICKS 5000
#define MAX_BUFFER_SIZE 256

static struct k_sem glcd_sem;

static jerry_value_t zjs_glcd_prototype;

static bool zjs_glcd_ipm_send_sync(zjs_ipm_message_t *send,
                                   zjs_ipm_message_t *result) {
    send->id = MSG_ID_GLCD;
    send->flags = 0 | MSG_SYNC_FLAG;
    send->user_data = (void *)result;
    send->error_code = ERROR_IPM_NONE;

    if (zjs_ipm_send(MSG_ID_GLCD, send) != 0) {
        ERR_PRINT("failed to send message\n");
        return false;
    }

    // block until reply or timeout, we shouldn't see the ARC
    // time out, if the ARC response comes back after it
    // times out, it could pollute the result on the stack
    if (k_sem_take(&glcd_sem, ZJS_GLCD_TIMEOUT_TICKS)) {
        ERR_PRINT("FATAL ERROR, ipm timed out\n");
        return false;
    }

    return true;
}

static jerry_value_t zjs_glcd_call_remote_function(zjs_ipm_message_t *send)
{
    if (!send)
        return zjs_error("zjs_glcd_call_remote_function: invalid send message");

    zjs_ipm_message_t reply;

    bool success = zjs_glcd_ipm_send_sync(send, &reply);

    if (!success) {
        return zjs_error("zjs_glcd_call_remote_function: ipm message failed or timed out!");
    }

    if (reply.error_code != ERROR_IPM_NONE) {
        ERR_PRINT("error code: %u\n", (unsigned int)reply.error_code);
        return zjs_error("zjs_glcd_call_remote_function: error received");
    }

    uint8_t value = reply.data.glcd.value;

    return jerry_create_number(value);
}

// returns undefined instead of the value result
static jerry_value_t zjs_glcd_call_remote_ignore(zjs_ipm_message_t *send)
{
    ZVAL rval = zjs_glcd_call_remote_function(send);
    if (jerry_value_has_error_flag(rval))
        return rval;

    return ZJS_UNDEFINED;
}

static void ipm_msg_receive_callback(void *context, uint32_t id, volatile void *data)
{
    if (id != MSG_ID_GLCD)
        return;

    zjs_ipm_message_t *msg = (zjs_ipm_message_t *)(*(uintptr_t *)data);

    if ((msg->flags & MSG_SYNC_FLAG) == MSG_SYNC_FLAG) {
         zjs_ipm_message_t *result = (zjs_ipm_message_t *)msg->user_data;
        // synchrounus ipm, copy the results
        if (result)
            memcpy(result, msg, sizeof(zjs_ipm_message_t));

        // un-block sync api
        k_sem_give(&glcd_sem);
    } else {
        // asynchronous ipm, should not get here
        ERR_PRINT("async message received\n");
    }
}

static ZJS_DECL_FUNC(zjs_glcd_print)
{
    // args: text
    ZJS_VALIDATE_ARGS(Z_STRING);

    jerry_size_t size = MAX_BUFFER_SIZE;
    char *buffer = zjs_alloc_from_jstring(argv[0], &size);
    if (!buffer) {
        return zjs_error("zjs_glcd_print: cannot allocate buffer");
    }

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_PRINT;
    send.data.glcd.buffer = buffer;

    jerry_value_t result = zjs_glcd_call_remote_ignore(&send);
    zjs_free(buffer);
    return result;
}

static ZJS_DECL_FUNC(zjs_glcd_clear)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_CLEAR;
    return zjs_glcd_call_remote_function(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_set_cursor_pos)
{
    // args: column, row
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_CURSOR_POS;
    send.data.glcd.col = (uint8_t)jerry_get_number_value(argv[0]);
    send.data.glcd.row = (uint8_t)jerry_get_number_value(argv[1]);

    return zjs_glcd_call_remote_ignore(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_select_color)
{
    // args: predefined color index
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SELECT_COLOR;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    return zjs_glcd_call_remote_ignore(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_set_color)
{
    // args: red, green, blue
    ZJS_VALIDATE_ARGS(Z_NUMBER, Z_NUMBER, Z_NUMBER);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_COLOR;
    send.data.glcd.color_r = (uint8_t)jerry_get_number_value(argv[0]);
    send.data.glcd.color_g = (uint8_t)jerry_get_number_value(argv[1]);
    send.data.glcd.color_b = (uint8_t)jerry_get_number_value(argv[2]);

    return zjs_glcd_call_remote_ignore(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_set_function)
{
    // args: predefined function number
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_FUNCTION;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    return zjs_glcd_call_remote_ignore(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_get_function)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_GET_FUNCTION;

    return zjs_glcd_call_remote_function(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_set_display_state)
{
    // args: predefined numeric constants
    ZJS_VALIDATE_ARGS(Z_NUMBER);

    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    send.type = TYPE_GLCD_SET_DISPLAY_STATE;
    send.data.glcd.value = (uint8_t)jerry_get_number_value(argv[0]);

    return zjs_glcd_call_remote_ignore(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_get_display_state)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_GET_DISPLAY_STATE;

    return zjs_glcd_call_remote_function(&send);
}

static ZJS_DECL_FUNC(zjs_glcd_init)
{
    // send IPM message to the ARC side
    zjs_ipm_message_t send;
    // no input parameter to set
    send.type = TYPE_GLCD_INIT;

    ZVAL result = zjs_glcd_call_remote_function(&send);
    if (jerry_value_has_error_flag(result)) {
        return result;
    }

    // create the Grove LCD device object
    jerry_value_t dev_obj = jerry_create_object();
    jerry_set_prototype(dev_obj, zjs_glcd_prototype);
    return dev_obj;
}

// Note. setInputState is not supported in Zephyr driver yet
// with right-to-left text flow (GLCD_IS_SHIFT_DECREMENT|GLCD_IS_ENTRY_RIGHT)
// and defaults to left-to-right only, so we don't support
// configuring input state until Zephyr implements this feature
jerry_value_t zjs_grove_lcd_init()
{
    zjs_ipm_init();
    zjs_ipm_register_callback(MSG_ID_GLCD, ipm_msg_receive_callback);

    k_sem_init(&glcd_sem, 0, 1);

    zjs_native_func_t array[] = {
        { zjs_glcd_print, "print" },
        { zjs_glcd_clear, "clear" },
        { zjs_glcd_set_cursor_pos, "setCursorPos" },
        { zjs_glcd_select_color, "selectColor" },
        { zjs_glcd_set_color, "setColor" },
        { zjs_glcd_set_function, "setFunction" },
        { zjs_glcd_get_function, "getFunction" },
        { zjs_glcd_set_display_state, "setDisplayState" },
        { zjs_glcd_get_display_state, "getDisplayState" },
        { NULL, NULL }
    };
    zjs_glcd_prototype = jerry_create_object();
    zjs_obj_add_functions(zjs_glcd_prototype, array);

    // create global grove_lcd object
    jerry_value_t glcd_obj = jerry_create_object();
    zjs_obj_add_function(glcd_obj, zjs_glcd_init, "init");

    // create object properties
    jerry_value_t val;

    // function flags
    val = jerry_create_number(GLCD_FS_8BIT_MODE);
    zjs_set_property(glcd_obj, "GLCD_FS_8BIT_MODE", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_ROWS_2);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_2", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_ROWS_1);
    zjs_set_property(glcd_obj, "GLCD_FS_ROWS_1", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_BIG);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_BIG", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_FS_DOT_SIZE_LITTLE);
    zjs_set_property(glcd_obj, "GLCD_FS_DOT_SIZE_LITTLE", val);
    jerry_release_value(val);

    // display state flags
    val = jerry_create_number(GLCD_DS_DISPLAY_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_DISPLAY_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_DISPLAY_OFF", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_CURSOR_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_CURSOR_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_CURSOR_OFF", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_BLINK_ON);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_ON", val);
    jerry_release_value(val);

    val = jerry_create_number(GLCD_DS_BLINK_OFF);
    zjs_set_property(glcd_obj, "GLCD_DS_BLINK_OFF", val);
    jerry_release_value(val);

    // colors
    val = jerry_create_number(GROVE_RGB_WHITE);
    zjs_set_property(glcd_obj, "GROVE_RGB_WHITE", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_RED);
    zjs_set_property(glcd_obj, "GROVE_RGB_RED", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_GREEN);
    zjs_set_property(glcd_obj, "GROVE_RGB_GREEN", val);
    jerry_release_value(val);

    val = jerry_create_number(GROVE_RGB_BLUE);
    zjs_set_property(glcd_obj, "GROVE_RGB_BLUE", val);
    jerry_release_value(val);

    return glcd_obj;
}

void zjs_grove_lcd_cleanup()
{
    jerry_release_value(zjs_glcd_prototype);
}

#endif // QEMU_BUILD
#endif // BUILD_MODULE_GROVE_LCD
