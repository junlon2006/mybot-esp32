/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_platform/mcp_trace.h"

#include <api/aosl_log.h>

#include "esp_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static aosl_vlog_t s_callback;
static int s_sdk_level;
static unsigned s_get_level_calls;
static unsigned s_set_level_calls;
static unsigned s_set_callback_calls;
static unsigned s_regular_calls;
static unsigned s_trace_calls;
static char s_regular_text[8192];
static char s_trace_text[8192];
static char s_trace_tag[64];
static char s_trace_severity;

int aosl_get_log_level(void) {
    ++s_get_level_calls;
    return s_sdk_level;
}

void aosl_set_log_level(int level) {
    ++s_set_level_calls;
    s_sdk_level = level;
}

void aosl_set_vlog_func(aosl_vlog_t callback) {
    ++s_set_callback_calls;
    s_callback = callback;
}

int mock_vprintf(const char *format, va_list args) {
    ++s_regular_calls;
    return vsnprintf(s_regular_text, sizeof(s_regular_text), format, args);
}

void mock_esp_log(char severity, const char *tag, const char *format, ...) {
    ++s_trace_calls;
    s_trace_severity = severity;
    snprintf(s_trace_tag, sizeof(s_trace_tag), "%s", tag);
    va_list args;
    va_start(args, format);
    vsnprintf(s_trace_text, sizeof(s_trace_text), format, args);
    va_end(args);
}

static void reset_output(void) {
    s_regular_calls = 0;
    s_trace_calls = 0;
    s_regular_text[0] = '\0';
    s_trace_text[0] = '\0';
    s_trace_tag[0] = '\0';
    s_trace_severity = '\0';
}

static void emit(int level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    s_callback(level, format, args);
    va_end(args);
}

static void check_args_preserved(int level, const char *format, ...) {
    char expected[256];
    char actual[256];
    va_list args;
    va_list copy;
    va_start(args, format);
    va_copy(copy, args);
    vsnprintf(expected, sizeof(expected), format, copy);
    va_end(copy);
    s_callback(level, format, args);
    vsnprintf(actual, sizeof(actual), format, args);
    va_end(args);
    CHECK(strcmp(expected, actual) == 0);
}

static void test_start(int initial_level, int regular_level) {
    s_sdk_level = initial_level;
    mybot_mcp_trace_start();
    CHECK(s_callback != NULL);
    CHECK(s_sdk_level == AOSL_LOG_DEBUG);
    CHECK(s_get_level_calls == 1);
    CHECK(s_set_level_calls == 1);
    CHECK(s_set_callback_calls == 1);
    CHECK(s_trace_calls == 1);
    CHECK(s_trace_severity == 'I');
    CHECK(strcmp(s_trace_tag, "mybot_mcp") == 0);
    char expected[64];
    snprintf(expected, sizeof(expected), "regular_sdk_level=%d", regular_level);
    CHECK(strstr(s_trace_text, expected) != NULL);

    mybot_mcp_trace_start();
    CHECK(s_get_level_calls == 1);
    CHECK(s_set_level_calls == 1);
    CHECK(s_set_callback_calls == 1);
    CHECK(s_trace_calls == 1);
}

static void test_normal_filtering(int regular_level) {
    for (int level = AOSL_LOG_EMERG; level <= AOSL_LOG_DEBUG; ++level) {
        reset_output();
        emit(level, "ordinary event value=%d\n", 42);
        CHECK(s_trace_calls == 0);
        CHECK(s_regular_calls == (unsigned)(level <= regular_level));
        if (s_regular_calls) {
            CHECK(strcmp(s_regular_text, "ordinary event value=42\n") == 0);
        }
    }

    reset_output();
    emit(AOSL_LOG_NOTICE, "ordinary event content=%s\n", "[RTM] is only an argument");
    CHECK(s_regular_calls == 1);
    CHECK(s_trace_calls == 0);
    CHECK(strcmp(s_regular_text, "ordinary event content=[RTM] is only an argument\n") == 0);
}

static void test_trace_severity(void) {
    for (int level = AOSL_LOG_EMERG; level <= AOSL_LOG_DEBUG; ++level) {
        reset_output();
        emit(level, "[aosl][handler:123][RTM] status=%d\r\n", 23);
        CHECK(s_regular_calls == 0);
        CHECK(s_trace_calls == 1);
        CHECK(strcmp(s_trace_tag, "mybot_mcp") == 0);
        char expected[128];
        snprintf(expected, sizeof(expected),
                 "event=rtm_trace sdk_level=%d truncated=0 [RTM] status=23", level);
        CHECK(strcmp(s_trace_text, expected) == 0);
        CHECK(s_trace_severity == (level <= AOSL_LOG_ERROR     ? 'E'
                                   : level == AOSL_LOG_WARNING ? 'W'
                                                               : 'I'));
    }
}

static void test_raw_payload_and_newlines(void) {
    reset_output();
    const char *payload = "{\"content\":\"volume 50% %s %n\"}";
    emit(AOSL_LOG_DEBUG, "[7][aosl][receive:%u][RTM] data len=%zu msg=%s\r\n\n\r", 99U,
         strlen(payload), payload);
    CHECK(s_regular_calls == 0);
    CHECK(s_trace_calls == 1);
    CHECK(strstr(s_trace_text, payload) != NULL);
    CHECK(strstr(s_trace_text, "receive:") == NULL);
    CHECK(strchr(s_trace_text, '\r') == NULL);
    CHECK(strchr(s_trace_text, '\n') == NULL);
    CHECK(strstr(s_trace_text, "sdk_level=7 truncated=0 [RTM] data") != NULL);

    reset_output();
    emit(AOSL_LOG_DEBUG, "[RTM]\r\n");
    CHECK(strcmp(s_trace_text, "event=rtm_trace sdk_level=7 truncated=0 [RTM]") == 0);
}

static void test_truncation(void) {
    char payload[5000];
    memset(payload, 'x', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    reset_output();
    emit(AOSL_LOG_DEBUG, "[aosl][RTM] data=%s\n", payload);
    CHECK(s_trace_calls == 1);
    CHECK(s_regular_calls == 0);
    CHECK(strstr(s_trace_text, "sdk_level=7 truncated=1 [RTM] data=") != NULL);
    CHECK(strlen(s_trace_text) < 2200);
    CHECK(strstr(s_trace_text, payload) == NULL);
}

static void test_null_format(void) {
    reset_output();
    emit(AOSL_LOG_ERROR, NULL);
    CHECK(s_trace_calls == 0);
    CHECK(s_regular_calls == 0);
}

static void test_caller_args(void) {
    reset_output();
    check_args_preserved(AOSL_LOG_NOTICE, "ordinary first=%d second=%d", 17, 81);
    CHECK(s_regular_calls == 1);
    CHECK(s_trace_calls == 0);
    reset_output();
    check_args_preserved(AOSL_LOG_DEBUG, "[RTM] first=%d second=%d", 19, 83);
    CHECK(s_regular_calls == 0);
    CHECK(s_trace_calls == 1);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    int initial_level = atoi(argv[1]);
    CHECK(initial_level >= AOSL_LOG_EMERG && initial_level <= AOSL_LOG_DEBUG);
    int regular_level = initial_level < AOSL_LOG_NOTICE ? AOSL_LOG_NOTICE : initial_level;
    test_start(initial_level, regular_level);
    test_normal_filtering(regular_level);
    test_trace_severity();
    test_raw_payload_and_newlines();
    test_truncation();
    test_null_format();
    test_caller_args();
    printf("MCP trace: 7 groups passed (initial SDK level %d)\n", initial_level);
    return 0;
}
