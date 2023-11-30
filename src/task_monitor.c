#include "task_monitor.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#if !defined(CONFIG_FREERTOS_USE_TRACE_FACILITY) || !defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    #error "configUSE_TRACE_FACILITY and configGENERATE_RUN_TIME_STATS must be defined as 1 in FreeRTOSConfig.h!"
#endif

#define MILLISECONDS_PER_SECOND 1000

// Configure task monitor
#define TASK_MONITOR_PERIOD_IN_MS (10 * MILLISECONDS_PER_SECOND) // Run-time statistics will refresh every 10 seconds
#define TASK_MONITOR_CORE_AFFINITY tskNO_AFFINITY

#define COLOR_BLACK   "30"
#define COLOR_RED     "31"
#define COLOR_GREEN   "32"
#define COLOR_YELLOW  "33"
#define COLOR_BLUE    "34"
#define COLOR_PURPLE  "35"
#define COLOR_CYAN    "36"
#define COLOR_WHITE   "37"

#define COLOR(COLOR) "\033[0;" COLOR "m"
#define RESET_COLOR  "\033[0m"

#define BLACK   COLOR(COLOR_BLACK)
#define RED     COLOR(COLOR_RED)
#define GREEN   COLOR(COLOR_GREEN)
#define YELLOW  COLOR(COLOR_YELLOW)
#define BLUE    COLOR(COLOR_BLUE)
#define PURPLE  COLOR(COLOR_PURPLE)
#define CYAN    COLOR(COLOR_CYAN)
#define WHITE   COLOR(COLOR_WHITE)

typedef int (*CompareFunction)(const TaskStatus_t *, const TaskStatus_t *);

static void swap_tasks(TaskStatus_t *task1, TaskStatus_t *task2)
{
    TaskStatus_t temp = *task1;
    *task1 = *task2;
    *task2 = temp;
}

static void generic_sort(TaskStatus_t *tasks_status_array, size_t number_of_tasks, CompareFunction compare)
{
    if (number_of_tasks > 1) {
        for (size_t i = 0; i <= number_of_tasks; ++i) {
            for (size_t k = i + 1; k < number_of_tasks; ++k) {
                if (compare(&tasks_status_array[i], &tasks_status_array[k])) {
                    swap_tasks(&tasks_status_array[i], &tasks_status_array[k]);
                }
            }
        }
    }
}

static int compare_by_runtime(const TaskStatus_t *task1, const TaskStatus_t *task2)
{
    return task1->ulRunTimeCounter < task2->ulRunTimeCounter;
}

static int compare_by_core(const TaskStatus_t *task1, const TaskStatus_t *task2)
{
    return task1->xCoreID > task2->xCoreID;
}

static void sort_tasks_by_runtime(TaskStatus_t *tasks_status_array, size_t number_of_tasks)
{
    generic_sort(tasks_status_array, number_of_tasks, compare_by_runtime);
}

static void sort_tasks_by_core(TaskStatus_t *tasks_status_array, size_t number_of_tasks)
{
    generic_sort(tasks_status_array, number_of_tasks, compare_by_core);

    size_t i;
    for (i = 0; tasks_status_array[i].xCoreID == 0; ++i);

    sort_tasks_by_runtime(tasks_status_array, i);
    sort_tasks_by_runtime(&tasks_status_array[i], number_of_tasks - i);
}

static uint32_t get_current_time_ms(void)
{
    return (xTaskGetTickCount() * 1000) / configTICK_RATE_HZ;
}

static float get_current_heap_free_percent(void)
{
    size_t current_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    return ((float) current_size / total_size) * 100.0;
}

static float get_minimum_heap_free_percent(void)
{
    size_t minimum_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    return ((float) minimum_size / total_size) * 100.0;
}

static const char* int_to_string(int number, char *string)
{
    sprintf(string, "%d", number);
    return string;
}

static const char* task_state_to_string(eTaskState state)
{
    switch (state) {
        case eRunning:
            return "Running";
        case eReady:
            return "Ready";
        case eBlocked:
            return "Blocked";
        case eSuspended:
            return "Suspended";
        case eDeleted:
            return "Deleted";
        case eInvalid:
            return "Invalid";
        default:
            return "Unknown state";
    }
}

static void task_status_monitor_task(void *params)
{
    while (true) {
        UBaseType_t number_of_tasks = uxTaskGetNumberOfTasks();
        TaskStatus_t *p_tasks_status_array = pvPortMalloc(number_of_tasks * sizeof(TaskStatus_t));

        if (p_tasks_status_array != NULL) {
            uint32_t total_run_time;
            number_of_tasks = uxTaskGetSystemState(p_tasks_status_array, number_of_tasks, &total_run_time);

            if (total_run_time > 0) {  // Avoid divide by zero error
                sort_tasks_by_core(p_tasks_status_array, number_of_tasks);

                printf("I (%lu) tm: " BLUE "%-18.16s %-11.10s %-6.5s %-8.8s %-10.9s %-11.10s %-15.15s %-s"
                    RESET_COLOR,
                    get_current_time_ms(),
                    "TASK NAME:",
                    "STATE:",
                    "CORE:",
                    "NUMBER:",
                    "PRIORITY:",
                    "STACK_MIN:",
                    "RUNTIME, µs:",
                    "RUNTIME, %:\n"
                );

                for (size_t i = 0; i < number_of_tasks; ++i) {
                    char string[10];  // 10 - maximum number of characters for int
                    const char* color = (p_tasks_status_array[i].xCoreID == 0) ? YELLOW : CYAN;
                    printf("I (%lu) tm: %s%-18.16s %-11.10s %-6.5s %-8d %-10d %-11lu %-14lu %-10.3f\n"
                        RESET_COLOR,
                        get_current_time_ms(),
                        color,
                        p_tasks_status_array[i].pcTaskName,
                        task_state_to_string(p_tasks_status_array[i].eCurrentState),

                        xTaskGetCoreID(p_tasks_status_array[i].xHandle) == tskNO_AFFINITY ?
                            "Any" : int_to_string((int)xTaskGetCoreID(p_tasks_status_array[i].xHandle), string),

                        p_tasks_status_array[i].xTaskNumber,
                        p_tasks_status_array[i].uxCurrentPriority,
                        p_tasks_status_array[i].usStackHighWaterMark,
                        p_tasks_status_array[i].ulRunTimeCounter,
                       (p_tasks_status_array[i].ulRunTimeCounter * 100.0) / total_run_time);
                }

                printf("I (%lu) tm: " YELLOW "Total heap free size:   " GREEN "%d" YELLOW " bytes\n" RESET_COLOR,
                    get_current_time_ms(), heap_caps_get_total_size(MALLOC_CAP_DEFAULT));

                printf("I (%lu) tm: " YELLOW "Current heap free size: " GREEN "%d" YELLOW " bytes (" GREEN "%.2f"
                    YELLOW " %%)\n" RESET_COLOR, get_current_time_ms(), heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                    get_current_heap_free_percent());

                printf("I (%lu) tm: " YELLOW "Minimum heap free size: " GREEN "%d" YELLOW " bytes (" GREEN "%.2f"
                    YELLOW " %%)\n" RESET_COLOR, get_current_time_ms(),
                    heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT), get_minimum_heap_free_percent());

                printf("I (%lu) tm: " YELLOW "Total RunTime: " GREEN "%lu" YELLOW " µs (" GREEN "%lu" YELLOW
                    " seconds)\n" RESET_COLOR, get_current_time_ms(), total_run_time, total_run_time / 1000000);

                uint64_t current_time = esp_timer_get_time();
                printf("I (%lu) tm: " YELLOW "System UpTime: " GREEN "%llu" YELLOW " µs (" GREEN "%llu" YELLOW
                    " seconds)\n\n" RESET_COLOR, get_current_time_ms(), current_time, current_time / 1000000);
            }
            vPortFree(p_tasks_status_array);
        } else {
            printf("I (%lu) tm: " RED "Could not allocate required memory\n" RESET_COLOR, get_current_time_ms());
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_MONITOR_PERIOD_IN_MS));
    }
}

esp_err_t task_monitor(void)
{
    BaseType_t status = xTaskCreatePinnedToCore(task_status_monitor_task, "monitor_task", configMINIMAL_STACK_SIZE * 4,
                                                NULL, tskIDLE_PRIORITY + 1, NULL, TASK_MONITOR_CORE_AFFINITY);
    if (status != pdPASS) {
        printf("I (%lu) tm: task_status_monitor_task(): Task was not created. Could not allocate required memory\n",
            get_current_time_ms());
        return ESP_ERR_NO_MEM;
    }
    printf("I (%lu) tm: task_status_monitor_task() started successfully\n", get_current_time_ms());
    return ESP_OK;
}
