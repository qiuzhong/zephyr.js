// Copyright (c) 2016-2017, Intel Corporation.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zjs_util.h"
#include "zjs_callbacks.h"

static int passed = 0;
static int total = 0;

static void zjs_assert(int test, char *str)
{
    char *label = "\033[1m\033[31mFAIL\033[0m";
    if (test) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    total += 1;
    printf("%s - %s\n", label, str);
}

// Test zjs_hex_to_byte function

static int check_hex_to_byte(char *str, uint8_t byte)
{
    uint8_t result;
    zjs_hex_to_byte(str, &result);
    return result == byte;
}

static uint32_t count1 = 0;
static void c_callback1(void *handle, const void *args)
{
    count1++;
}

static uint8_t string_correct = 0;
static void c_callback2(void *handle, const void *args)
{
    char *s = (char *)handle;
    if (strcmp(s, "pass string as handle") == 0) {
        string_correct = 1;
    }
}

static uint8_t handle_and_args = 0;
static void c_callback3(void *handle, const void *args)
{
    char *h = (char *)handle;
    char *a = (char *)args;
    if (strcmp(h, "this is my handle") == 0) {
        if (strcmp(a, "this is my args") == 0) {
            handle_and_args = 1;
        }
    }
}

static uint8_t cb4_called = 0;
static void c_callback4(void *handle, const void *args)
{
    cb4_called = 1;
}

static uint8_t handle_correct = 0;
static void c_callback5(void *handle, const void *args)
{
    uint32_t h = *((uint32_t *)handle);
    if (h == 0x44332211) {
        handle_correct = 1;
    }

}

static void test_c_callbacks()
{
    zjs_init_callbacks();
    // test multiple signals
    zjs_callback_id id1 = zjs_add_c_callback(NULL, c_callback1);
    for (int i = 0; i < 10; i++) {
        zjs_signal_callback(id1, NULL, 0);
    }
    zjs_service_callbacks();
    zjs_assert(count1 == 10, "signal callback 10 times");
    zjs_remove_callback(id1);

    // test handle
    char s[] = "pass string as handle";
    zjs_callback_id id2 = zjs_add_c_callback(s, c_callback2);
    zjs_signal_callback(id2, NULL, 0);
    zjs_service_callbacks();
    zjs_assert(string_correct, "handle passed correctly");
    zjs_remove_callback(id2);

    // test args and handle
    char h[] = "this is my handle";
    char a[] = "this is my args";
    zjs_callback_id id3 = zjs_add_c_callback(h, c_callback3);
    zjs_signal_callback(id3, a, strlen(a));
    zjs_service_callbacks();
    zjs_assert(handle_and_args, "handle and args passed correctly");
    zjs_remove_callback(id3);

    // test callback ID recycling
    zjs_callback_id list[10];
    for (int i = 0; i < 10; i++) {
        list[i] = zjs_add_c_callback(NULL, c_callback4);
    }
    zjs_callback_id next = zjs_add_c_callback(NULL, c_callback4);
    for (int i = 0; i < 10; i++) {
        zjs_remove_callback(list[i]);
    }
    zjs_service_callbacks();
    zjs_callback_id less = zjs_add_c_callback(NULL, c_callback4);
    zjs_assert(less < next, "callback IDs are recycled");
    zjs_remove_callback(next);
    zjs_remove_callback(less);

    // test zjs_call_callback
    zjs_callback_id id4 = zjs_add_c_callback(NULL, c_callback4);
    zjs_call_callback(id4, NULL, 0);
    zjs_assert(cb4_called, "zjs_call_callback()");
    zjs_remove_callback(id4);

    // test zjs_edit_callback_handle()
    uint32_t h1 = 0x11223344;
    uint32_t h2 = 0x44332211;
    zjs_callback_id id5 = zjs_add_c_callback((void *)&h1, c_callback5);
    zjs_signal_callback(id5, NULL, 0);
    zjs_edit_callback_handle(id5, (void *)&h2);
    zjs_service_callbacks();
    zjs_assert(handle_correct, "zjs_edit_callback_handle()");
}

static void test_hex_to_byte()
{
    zjs_assert(check_hex_to_byte("00", 0), "hex to byte: 00");
    zjs_assert(check_hex_to_byte("ff", 0xff), "hex to byte: ff");
    zjs_assert(check_hex_to_byte("FF", 0xff), "hex to byte: FF");
    zjs_assert(check_hex_to_byte("3a", 0x3a), "hex to byte: 3a");
    zjs_assert(check_hex_to_byte("b4", 0xb4), "hex to byte: b4");
    zjs_assert(check_hex_to_byte("Aa", 0xaa), "hex to byte: Aa");
}

// Test zjs_default_convert_pin function

static void test_default_convert_pin()
{
    int dev, pin;
    zjs_default_convert_pin(0x00, &dev, &pin);
    zjs_assert(dev == 0 && pin == 0, "convert pin 00 -> 0/0");
    zjs_default_convert_pin(0x27, &dev, &pin);
    zjs_assert(dev == 1 && pin == 7, "convert pin 27 -> 1/7");
    zjs_default_convert_pin(0xef, &dev, &pin);
    zjs_assert(dev == 7 && pin == 15, "convert pin fe -> 7/15");
    zjs_default_convert_pin(0xfe, &dev, &pin);
    zjs_assert(dev == 7 && pin == 30, "convert pin fe -> 7/31");
    zjs_default_convert_pin(0xff, &dev, &pin);
    zjs_assert(dev == 0 && pin == -1, "convert pin failure case");
}

// Test zjs_util int compression functions

static int check_compress_reversible(uint32_t num)
{
    // checks whether uncompressing compressed value returns exact match
    uint32_t reversed = zjs_uncompress_16_to_32(zjs_compress_32_to_16(num));
    return num == reversed;
}

static int check_compress_close(uint32_t num)
{
    // checks whether uncompressing compressed value is within 0.05% of original
    // 11-bit mantissa means 1/2048 ~= 0.05%
    uint32_t reversed = zjs_uncompress_16_to_32(zjs_compress_32_to_16(num));
    double ratio = reversed * 1.0 / num;
    return ratio >= 0.9995 && ratio <= 1.0005;
}

static void test_compress_32()
{
    // expect these to be returned exactly
    zjs_assert(check_compress_reversible(0), "compression of 0");
    zjs_assert(check_compress_reversible(0x0cab), "compression of 0x0cab");
    zjs_assert(check_compress_reversible(0x7fff), "compression of 0x7fff");

    // these should be close
    zjs_assert(check_compress_close(0x8000), "compression of 0x8000");
    zjs_assert(check_compress_close(0xdeadbeef), "compression of 0xdeadbeef");
    zjs_assert(check_compress_close(0xffffffff), "compression of 0xffffffff");
}

// Test test_validate_args function

static ZJS_DECL_FUNC(dummy_func)
{
    return ZJS_UNDEFINED;
}

static void test_validate_args()
{
    // create some dummy objects
    ZVAL obj1 = jerry_create_object();
    ZVAL str1 = jerry_create_string((jerry_char_t *)"arthur");
    ZVAL func1 = jerry_create_external_function(dummy_func);
    ZVAL array1 = jerry_create_array(3);
    ZVAL num1 = jerry_create_number(42);
    ZVAL bool1 = jerry_create_boolean(true);
    ZVAL null1 = jerry_create_null();
    ZVAL undef1 = jerry_create_undefined();

    // create expect strings
    const char *expect_none[]  = { NULL };
    const char *expect_array[] = { Z_ARRAY, NULL };
    const char *expect_bool[]  = { Z_BOOL, NULL };
    const char *expect_func[]  = { Z_FUNCTION, NULL };
    const char *expect_null[]  = { Z_NULL, NULL };
    const char *expect_num[]   = { Z_NUMBER, NULL };
    const char *expect_obj[]   = { Z_OBJECT, NULL };
    const char *expect_str[]   = { Z_STRING, NULL };
    const char *expect_undef[] = { Z_UNDEFINED, NULL };

    // low expectations
    zjs_assert(zjs_validate_args(expect_none, 0, NULL) == 0,
               "none asked, none given");

    jerry_value_t argv[5];
    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_none, 1, argv) == 0,
               "expect none, pass the pi");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_none, 1, argv) == 0,
               "expect none, pass an object");

    // expect a string
    zjs_assert(zjs_validate_args(expect_str, 0, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect string, pass nothing");

    argv[0] = str1;
    zjs_assert(zjs_validate_args(expect_str, 1, argv) == 0,
               "expect string, pass string");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_str, 1, argv) == ZJS_INVALID_ARG,
               "expect string, pass object");

    argv[0] = str1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_str, 2, argv) == 0,
               "expect string, pass string and object");

    // expect an object
    zjs_assert(zjs_validate_args(expect_obj, 0, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect object, pass nothing");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass object");

    argv[0] = func1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass function");

    argv[0] = array1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == 0,
               "expect object, pass array");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_obj, 1, argv) == ZJS_INVALID_ARG,
               "expect object, pass number");

    // expect a function
    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_func, 1, argv) == ZJS_INVALID_ARG,
               "expect function, pass object");

    argv[0] = func1;
    zjs_assert(zjs_validate_args(expect_func, 1, argv) == 0,
               "expect function, pass function");

    // expect an array
    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_array, 1, argv) == ZJS_INVALID_ARG,
               "expect array, pass object");

    argv[0] = array1;
    zjs_assert(zjs_validate_args(expect_array, 1, argv) == 0,
               "expect array, pass array");

    // expect a number
    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_num, 1, argv) == 0,
               "expect number, pass number");

    argv[0] = null1;
    zjs_assert(zjs_validate_args(expect_num, 1, argv) == ZJS_INVALID_ARG,
               "expect number, pass null");

    // expect a bool
    argv[0] = bool1;
    zjs_assert(zjs_validate_args(expect_bool, 1, argv) == 0,
               "expect bool, pass bool");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_bool, 1, argv) == ZJS_INVALID_ARG,
               "expect bool, pass number");

    // expect a null
    argv[0] = null1;
    zjs_assert(zjs_validate_args(expect_null, 1, argv) == 0,
               "expect null, pass null");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == ZJS_INVALID_ARG,
               "expect null, pass number");

    // expect an undefined
    argv[0] = undef1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == 0,
               "expect undef, pass undef");

    argv[0] = num1;
    zjs_assert(zjs_validate_args(expect_undef, 1, argv) == ZJS_INVALID_ARG,
               "expect undef, pass number");

    // bogus expectation strings
    char too_low[2] = Z_ANY;
    too_low[0]--;
    const char *expect_low[] = { too_low, NULL };
    zjs_assert(zjs_validate_args(expect_low, 1, argv) == ZJS_INTERNAL_ERROR,
               "pass low expectation character");

    char too_high[2] = Z_UNDEFINED;
    too_high[0]++;
    const char *expect_high[] = { too_high, NULL };
    zjs_assert(zjs_validate_args(expect_high, 1, argv) == ZJS_INTERNAL_ERROR,
               "pass high expectation character");

    // optional argument
    const char *expect_opt[] = { Z_OPTIONAL Z_NUMBER, Z_OBJECT, NULL };
    argv[0] = num1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_opt, 2, argv) == 1,
               "expect optional number, required object, pass both");

    zjs_assert(zjs_validate_args(expect_opt, 1, argv) == ZJS_INSUFFICIENT_ARGS,
               "expect optional number, required object, pass number");

    argv[0] = obj1;
    zjs_assert(zjs_validate_args(expect_opt, 1, argv) == 0,
               "expect optional number, required object, pass object");

    // two optional arguments
    const char *expect_more[] = { Z_OPTIONAL Z_NUMBER, Z_OPTIONAL Z_NULL,
                                Z_OBJECT, NULL };
    zjs_assert(zjs_validate_args(expect_more, 1, argv) == 0,
               "optional number and null, required object, pass object");

    argv[0] = num1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 2, argv) == 1,
               "optional number and null, required object, pass num + obj");

    argv[0] = null1;
    argv[1] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 2, argv) == 1,
               "optional number and null, required object, pass null + obj");

    argv[0] = num1;
    argv[1] = null1;
    argv[2] = obj1;
    zjs_assert(zjs_validate_args(expect_more, 3, argv) == 2,
               "optional number and null, required object, pass all");
}

void zjs_run_unit_tests()
{
    test_hex_to_byte();
    test_default_convert_pin();
    test_compress_32();
    test_validate_args();
    test_c_callbacks();

    printf("TOTAL - %d of %d passed\n", passed, total);
    exit(!(passed == total));
}
