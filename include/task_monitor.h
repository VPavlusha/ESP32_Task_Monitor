/*
 * MIT License
 *
 * Copyright (c) 2023 Volodymyr Pavlusha
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TASK_MONITOR_H_
#define TASK_MONITOR_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: configUSE_TRACE_FACILITY and configGENERATE_RUN_TIME_STATS must be defined as 1
// in FreeRTOSConfig.h for this API function task_monitor() to be available.

/**
 * @brief Create a new task to monitor the status and activity of other FreeRTOS tasks in the system
 *
 * @return
 *      - ESP_OK           Success.
 *      - ESP_ERR_NO_MEM   Out of memory.
 */
esp_err_t task_monitor(void);

#ifdef __cplusplus
}
#endif

#endif  // TASK_MONITOR_H_
