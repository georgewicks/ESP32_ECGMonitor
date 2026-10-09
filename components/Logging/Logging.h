/**
 * @file Logging.h
 * @brief Mirror ESP-IDF log output (ESP_LOGI/ESP_LOGW/ESP_LOGE/...) to a file,
 *        in addition to the UART.
 *
 * Usage:
 *   1. Mount the filesystem (e.g. the SD card).
 *   2. Call logging_start("/sdcard/debug.log").
 *   3. All subsequent ESP_LOG calls are also appended to that file, and the
 *      file is flushed + fsynced about once per second so it survives a
 *      power cut.
 *   4. Call logging_stop() to close the file and stop mirroring.
 *
 * Future algorithm work only needs to use the normal ESP_LOG macros; once
 * this module is started, that output is captured automatically.
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open @p path for append and start mirroring ESP_LOG output to it.
 *
 * @param path   Absolute path to the log file (e.g. "/sdcard/debug.log").
 *               Pass NULL to stop logging.
 * @return       The opened FILE*, or NULL on failure.
 *
 * Note: the log hook is installed once and remains installed after this
 * returns; it simply stops writing when logging_stop() is called.
 */
FILE *logging_start(const char *path);

/** Flush + fsync the log file immediately (normally done automatically ~1/s). */
void logging_flush(void);

/** Close the log file and stop mirroring ESP_LOG output to it. */
void logging_stop(void);

#ifdef __cplusplus
}
#endif
