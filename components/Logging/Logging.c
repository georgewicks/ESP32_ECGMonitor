/**
 * @file Logging.c
 * @brief Implementation of the ESP_LOG -> file mirror (see Logging.h).
 *
 * ESP-IDF serializes all log output through a single vprintf-like function
 * (esp_log_set_vprintf). We install our own that keeps the normal UART output
 * and additionally appends each line to the configured file. The ESP-IDF log
 * core holds a lock around this call, so no extra synchronization is needed.
 *
 * Do not call ESP_LOG from an interrupt context while a file is attached:
 * fprintf from an ISR is not permitted.
 */

#include "Logging.h"

#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

#include "esp_log.h"

static FILE *s_log_file = NULL;

static int log_vprintf(const char *fmt, va_list args)
{
    va_list ua;
    int ret = 0;

    /* Preserve the default UART output. */
    va_copy(ua, args);
    ret = vprintf(fmt, ua);
    va_end(ua);

    if (s_log_file != NULL) {
        va_list fa;
        va_copy(fa, args);
        vfprintf(s_log_file, fmt, fa);
        va_end(fa);

        /* Flush + fsync about once per second so the log survives power cuts,
         * without paying the cost of an fsync on every single line. */
        static uint32_t last_flush_ms = 0;
        uint32_t now = esp_log_timestamp();
        if ((uint32_t)(now - last_flush_ms) >= 1000) {
            fflush(s_log_file);
            fsync(fileno(s_log_file));
            last_flush_ms = now;
        }
    }

    return ret;
}

FILE *logging_start(const char *path)
{
    logging_stop();

    if (path == NULL) {
        return NULL;
    }

    FILE *f = fopen(path, "a");
    if (f == NULL) {
        return NULL;
    }

    /* Line-buffered: each ESP_LOG line ends with '\n', so it reaches the
     * filesystem promptly; the 1 s fsync above commits it to the media. */
    setvbuf(f, NULL, _IOLBF, 0);

    s_log_file = f;

    static bool hook_installed = false;
    if (!hook_installed) {
        esp_log_set_vprintf(log_vprintf);
        hook_installed = true;
    }

    return f;
}

void logging_flush(void)
{
    if (s_log_file != NULL) {
        fflush(s_log_file);
        fsync(fileno(s_log_file));
    }
}

void logging_stop(void)
{
    if (s_log_file != NULL) {
        fflush(s_log_file);
        fsync(fileno(s_log_file));
        fclose(s_log_file);
        s_log_file = NULL;
    }
}
